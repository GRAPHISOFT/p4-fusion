/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include <algorithm>
#include <thread>
#include <map>
#include <atomic>
#include <memory>
#include <mutex>
#include <regex>
#include <fstream>
#include <sstream>
#include <typeinfo>
#include <csignal>
#include <iterator>
#include <fstream>
#include <aws/core/Aws.h>

#include "common.h"

#include "utils/std_helpers.h"
#include "utils/timer.h"
#include "utils/arguments.h"
#include "utils/p4_depot_path.h"

#include "thread_pool.h"
#include "p4_api.h"
#include "git_api.h"
#include "branch_set.h"
#include "lfs/lfs_client.h"
#include "lfs/communication/lfscomm.h"
#include "lfs/communication/s3comm.h"

#include "p4/p4libs.h"
#include "minitrace.h"

#define P4_FUSION_VERSION "v1.13.0"

void SignalHandler(sig_atomic_t s);

int Main(int argc, char** argv)
{
	Timer programTimer;

	Arguments::GetSingleton()->RequiredParameter("--path", "P4 depot path to convert to a Git repo.  If used with '--branch', this is the base path for the branches.");
	Arguments::GetSingleton()->RequiredParameter("--src", "Relative path where the git repository should be created. This path should be empty before running p4-fusion for the first time in a directory.");
	Arguments::GetSingleton()->RequiredParameter("--port", "Specify which P4PORT to use.");
	Arguments::GetSingleton()->RequiredParameter("--user", "Specify which P4USER to use. Please ensure that the user is logged in.");
	Arguments::GetSingleton()->RequiredParameter("--client", "Name/path of the client workspace specification.");
	Arguments::GetSingleton()->RequiredParameter("--lookAhead", "How many CLs in the future, at most, shall we keep downloaded by the time it is to commit them?");
	Arguments::GetSingleton()->RequiredParameter("--caseHandling", "Case handling of the Perforce server, as reported by the 'Case Handling' line of 'p4 info': 'insensitive' or 'sensitive'. It controls how depot paths are matched (client view, '--path', '--branch', stream mappings and merge/move detection). Only 'insensitive' is tested and supported; 'sensitive' is rejected.");
	Arguments::GetSingleton()->OptionalParameterList("--branch", "A branch to migrate under the depot path.  May be specified more than once.  If at least one is given and the noMerge option is false, then the Git repository will include merges between branches in the history.  You may use the formatting 'depot/path:git-alias', separating the Perforce branch sub-path from the git alias name by a ':'; if the depot path contains a ':', then you must provide the git branch alias.");
	Arguments::GetSingleton()->OptionalParameter("--noMerge", "false", "Disable performing a Git merge when a Perforce branch integrates (or copies, etc) into another branch.");
	Arguments::GetSingleton()->OptionalParameter("--mergeDeletes", "true", "When enabled, files that are deleted or moved on a target branch as part of an integration changelist are folded into the merge commit rather than being placed in a separate non-merge commit. Only affects runs where --branch mappings are also specified; without branches there are no merge commits, so this option has no effect.");
	Arguments::GetSingleton()->OptionalParameter("--networkThreads", std::to_string(std::thread::hardware_concurrency()), "Specify the number of threads in the threadpool for running network calls. Defaults to the number of logical CPUs.");
	Arguments::GetSingleton()->OptionalParameter("--printBatch", "1", "Specify the p4 print batch size.");
	Arguments::GetSingleton()->OptionalParameter("--maxChanges", "-1", "Specify the max number of changelists which should be processed in a single run. -1 signifies unlimited range.");
	Arguments::GetSingleton()->OptionalParameter("--retries", "10", "Specify how many times a command should be retried before the process exits in a failure.");
	Arguments::GetSingleton()->OptionalParameter("--refresh", "100", "Specify how many times a connection should be reused before it is refreshed.");
	Arguments::GetSingleton()->OptionalParameter("--fsyncEnable", "false", "Enable fsync() while writing objects to disk to ensure they get written to permanent storage immediately instead of being cached. This is to mitigate data loss in events of hardware failure.");
	Arguments::GetSingleton()->OptionalParameter("--includeBinaries", "false", "Do not discard binary files while downloading changelists.");
	Arguments::GetSingleton()->OptionalParameter("--flushRate", "1000", "Rate at which profiling data is flushed on the disk.");
	Arguments::GetSingleton()->OptionalParameter("--repackRate", "0", "How often (in number of CLs) to perform an incremental repack.");
	Arguments::GetSingleton()->OptionalParameter("--noColor", "false", "Disable colored output.");
	Arguments::GetSingleton()->OptionalParameterList("--exclude", "A regex used to exclude files from the conversion. Can be specified more than once.\n"
	                                                              "\tExamples:\n"
	                                                              "\t\tExclude all files in Tools/Development, except AModule.ini and files in AModule, AModuleTest: --exclude \"//Tools/Development/(?!AModule/.*|AModuleTest/.*|AModule\\.ini).*\"\n"
	                                                              "\t\tExclude all .pyc files: --exclude \".*\\.pyc\"\n"
	                                                              "\t\tExclude a specific file: --exclude \"//Tools/Development/BModule/OpenCppCoverageSetup-x64-0.9.9.0.exe\"\n");
	Arguments::GetSingleton()->OptionalParameter("--excludeLog", "false", "Log the excluded files in the folder given by '--logFolderPath', one '<CL> <reason> <depotFile>#<rev>' line per excluded file. Reason is 'regex' (matches an '--exclude'), 'binary' (binary file skipped because '--includeBinaries' is not set), 'path' (it's on a CL but not under '--path') or 'branch' (it's under '--path' but not under any '--branch'). Requires '--logFolderPath'.");
	Arguments::GetSingleton()->OptionalParameter("--includeLog", "false", "Log the files to be committed in the folder given by '--logFolderPath', one '<CL> <kind> <gitBranch> <p4action> <depotFile>#<rev>' line per file. Kind is 'git' (contents committed to the git repo), 'lfs' (matches an '--lfsSpec', committed as LFS pointer) or 'del' (deleted file). gitBranch is '-' when no '--branch' is used. Requires '--logFolderPath'.");
	Arguments::GetSingleton()->OptionalParameter("--dryRun", "false", "Process the changelists without downloading file contents ('p4 print'), uploading to LFS or touching the git repository. An existing repository at '--src' is only read to resume from its last converted CL. Only the run, exclude and include logs are written; enables '--excludeLog' and '--includeLog'. Requires '--logFolderPath'.");
	Arguments::GetSingleton()->OptionalParameter("--streamMappings", "false", "Use Mappings defined by Perforce Stream Spec for a given stream");
	Arguments::GetSingleton()->OptionalParameterList("--lfsSpec", "Path spec for files to be handled by Git LFS. Can be specified more than once.");
	Arguments::GetSingleton()->OptionalParameter("--lfsServerUrl", "", "URL of the Git LFS server to use for uploading files with basic transfer.");
	Arguments::GetSingleton()->OptionalParameter("--lfsUsername", "", "Git LFS username for basic access authentication.");
	Arguments::GetSingleton()->OptionalParameter("--lfsPassword", "", "Git LFS password for basic access authentication.");
	Arguments::GetSingleton()->OptionalParameter("--lfsToken", "", "Git LFS token for authentication.");
	Arguments::GetSingleton()->OptionalParameter("--lfsAPI", "lfs", "Specify the type of the used LFS server API. The types currently supported are 'lfs' (the default) and 's3'.");
	Arguments::GetSingleton()->OptionalParameter("--lfsS3Bucket", "", "Specify the name of the S3 bucket to use for LFS storage.");
	Arguments::GetSingleton()->OptionalParameter("--lfsS3Repository", "", "Specify the name of the repository used to store LFS files in S3 bucket.");
	Arguments::GetSingleton()->OptionalParameterList("--overrideToText", "Path spec for files to be handled as text, even when their P4 type is binary or something else. "
	                                                                     "Normally this results in them being committed to the Git repo instead of ignored. "
	                                                                     "In includeBinaries+LFS mode, the LFS pathspecs control where to commit what; in that case this only serves to silence a warning."
	                                                                     "Can be specified more than once.");
	Arguments::GetSingleton()->OptionalParameterList("--overrideToBinary", "Path spec for files to be handled as binary, even when their P4 type is something else. "
	                                                                       "Normally this results in them being ignored instead of committed. "
	                                                                       "In includeBinaries+LFS mode, the LFS pathspecs control where to commit what; in that case this does nothing."
	                                                                       "Can be specified more than once.");
	Arguments::GetSingleton()->OptionalParameter("--verbose", "false", "Enable verbose logging.");
	Arguments::GetSingleton()->OptionalParameter("--logFolderPath", "", "Path to an existing folder for the log files. When set, all log output is written to this folder. Logging into file always uses verbose mode.");

	PRINT("p4-fusion " P4_FUSION_VERSION);

	Arguments::GetSingleton()->Initialize(argc, argv);
	if (!Arguments::GetSingleton()->IsValid())
	{
		PRINT("Usage:" + Arguments::GetSingleton()->Help());
		return 0;
	}

	const bool noColor = Arguments::GetSingleton()->GetNoColor() != "false";
	if (noColor)
	{
		Log::DisableColoredOutput();
	}

	const bool verbose = Arguments::GetSingleton()->GetVerbose() != "false";
	if (verbose)
	{
		Log::SetLogLevel(Log::LogLevel::Verbose);
	}

	const std::string logFolderPath = Arguments::GetSingleton()->GetLogFolderPath();
	if (!logFolderPath.empty())
	{
		if (!Log::StartLogFile(logFolderPath))
		{
			ERR("Failed to create log file in folder: " << logFolderPath);
			return 1;
		}
	}

	const bool noMerge = Arguments::GetSingleton()->GetNoMerge() != "false";
	const bool mergeDeletes = Arguments::GetSingleton()->GetMergeDeletes() != "false";

	const std::string caseHandling = Arguments::GetSingleton()->GetCaseHandling();
	if (caseHandling == "sensitive")
	{
		ERR("'--caseHandling sensitive' (case-sensitive Perforce server) is not tested and not supported. Exiting.");
		return 1;
	}
	if (caseHandling != "insensitive")
	{
		ERR("Invalid '--caseHandling' value '" << caseHandling << "'. Supported values are 'insensitive' and 'sensitive'. Exiting.");
		return 1;
	}
	const bool caseInsensitive = caseHandling == "insensitive";
	P4API::CaseInsensitive = caseInsensitive;

	const std::string depotPath = Arguments::GetSingleton()->GetDepotPath();
	const std::string srcPath = Arguments::GetSingleton()->GetSourcePath();
	const bool fsyncEnable = Arguments::GetSingleton()->GetFsyncEnable() != "false";
	const bool includeBinaries = Arguments::GetSingleton()->GetIncludeBinaries() != "false";
	const int maxChanges = std::atoi(Arguments::GetSingleton()->GetMaxChanges().c_str());
	const int flushRate = std::atoi(Arguments::GetSingleton()->GetFlushRate().c_str());
	const int repackRate = std::atoi(Arguments::GetSingleton()->GetRepackRate().c_str());
	const std::vector<std::string> branchNames = Arguments::GetSingleton()->GetBranches();
	const std::vector<std::string> excludesStrs = Arguments::GetSingleton()->GetExcludes();
	const bool dryRun = Arguments::GetSingleton()->GetDryRun() != "false";
	const bool excludeLog = dryRun || Arguments::GetSingleton()->GetExcludeLog() != "false";
	const bool includeLog = dryRun || Arguments::GetSingleton()->GetIncludeLog() != "false";

	if (dryRun && logFolderPath.empty())
	{
		ERR("'--dryRun' requires '--logFolderPath' to be set.");
		return 1;
	}

	std::string excludeLogPath;
	std::unique_ptr<std::ofstream> excludeLogStream;
	if (excludeLog)
	{
		if (logFolderPath.empty())
		{
			ERR("'--excludeLog' requires '--logFolderPath' to be set.");
			return 1;
		}

		excludeLogPath = Log::GetLogFilePath(logFolderPath, "exclude");
		excludeLogStream.reset(new std::ofstream(excludeLogPath, std::ios::out | std::ios::app));
		if (!excludeLogStream->is_open())
		{
			ERR("Failed to open exclude log file: " << excludeLogPath);
			return 1;
		}
	}

	std::string includeLogPath;
	std::unique_ptr<std::ofstream> includeLogStream;
	if (includeLog)
	{
		if (logFolderPath.empty())
		{
			ERR("'--includeLog' requires '--logFolderPath' to be set.");
			return 1;
		}

		includeLogPath = Log::GetLogFilePath(logFolderPath, "include");
		includeLogStream.reset(new std::ofstream(includeLogPath, std::ios::out | std::ios::app));
		if (!includeLogStream->is_open())
		{
			ERR("Failed to open include log file: " << includeLogPath);
			return 1;
		}
	}

	std::vector<std::regex> excludes;
	for (const auto& excludeStr : excludesStrs)
	{
		try
		{
			excludes.emplace_back(excludeStr, std::regex_constants::icase);
		}
		catch (const std::regex_error& e)
		{
			ERR("Invalid regex '" << excludeStr << "': " << e.what());

			return 1;
		}
	}
	const bool streamMappings = Arguments::GetSingleton()->GetStreamMappings() != "false";

	const std::string lfsServerUrl = Arguments::GetSingleton()->GetLFSServerUrl();
	const std::string lfsUsername = Arguments::GetSingleton()->GetLFSUsername();
	const std::string lfsPassword = Arguments::GetSingleton()->GetLFSPassword();
	const std::string lfsToken = Arguments::GetSingleton()->GetLFSToken();
	const std::string lfsAPI = Arguments::GetSingleton()->GetLFSAPI();
	const std::string lfsS3Bucket = Arguments::GetSingleton()->GetLFSS3Bucket();
	const std::string lfsS3Repository = Arguments::GetSingleton()->GetLFSS3Repository();

	std::vector<std::string> lfsPatterns = Arguments::GetSingleton()->GetLFSSpecs();

	GitAPI git(fsyncEnable);
	std::unique_ptr<LFSClient> lfsClient;
	if (dryRun && !lfsPatterns.empty())
	{
		// Only used for classifying files as LFS tracked; there is no communicator, so nothing is uploaded.
		lfsClient.reset(new LFSClient(git, nullptr, lfsPatterns));

		PRINT("Initialized LFS client for dry run (classification only, no upload)");

		if (!includeBinaries)
		{
			WARN("LFS support is enabled, but binaries are excluded. This is probably not what you want.");
		}
	}
	else if (!lfsPatterns.empty() && !lfsServerUrl.empty() && !lfsAPI.empty())
	{
		std::unique_ptr<Communicator> communicator;
		if (lfsAPI == "s3")
		{
			communicator.reset(new S3Comm(lfsServerUrl, lfsS3Bucket, lfsS3Repository, lfsUsername, lfsPassword));
		}
		else if (lfsAPI == "lfs")
		{
			Credentials creds = lfsToken.empty() ? Credentials(lfsUsername, lfsPassword) : Credentials(lfsToken);
			communicator.reset(new LFSComm(lfsServerUrl, creds));
		}
		lfsClient.reset(new LFSClient(git, std::move(communicator), lfsPatterns));

		PRINT("Initialized LFS client with server URL: " << lfsServerUrl << " and API type: " << lfsAPI);

		if (!includeBinaries)
		{
			WARN("LFS support is enabled, but binaries are excluded. This is probably not what you want.");
		}
	}

	std::vector<std::string> overrideToTextSpecs = Arguments::GetSingleton()->GetOverrideToTextSpecs();
	std::vector<std::string> overrideToBinarySpecs = Arguments::GetSingleton()->GetOverrideToBinarySpecs();

	PRINT("Running p4-fusion from: " << argv[0]);

	if (!P4API::InitializeLibraries())
	{
		return 1;
	}
	// Set the signal here because it gets reset after P4API library is initialized
	std::signal(SIGINT, SignalHandler);
	std::signal(SIGTERM, SignalHandler);

	P4API::P4PORT = Arguments::GetSingleton()->GetPort();
	P4API::P4USER = Arguments::GetSingleton()->GetUsername();

	const Error serviceConnectionResult = P4API().TestConnection(5)->GetError();
	bool serverAvailable = serviceConnectionResult.IsError() == 0;
	if (serverAvailable)
	{
		SUCCESS("Perforce server is available");
	}
	else
	{
		ERR("Error occurred while connecting to " << P4API::P4PORT);
		return 1;
	}
	P4API::P4CLIENT = Arguments::GetSingleton()->GetClient();
	P4API::ClientSpec = P4API().Client()->GetClientSpec();

	if (P4API::ClientSpec.mapping.empty())
	{
		WARN("Received a client spec with no mappings. Did you use the correct corresponding P4PORT for the " + P4API::ClientSpec.client + " client spec?");
	}

	PRINT("Updated client workspace view " << P4API::ClientSpec.client << " with " << P4API::ClientSpec.mapping.size() << " mappings");

	P4API p4;

	if (!p4.IsDepotPathValid(depotPath))
	{
		ERR("Depot path should begin with \"//\" and end with \"/...\". Please pass in the proper depot path and try again.");
		return 1;
	}

	if (!p4.IsDepotPathUnderClientSpec(depotPath))
	{
		ERR("The depot path specified is not under the " << P4API::ClientSpec.client << " client spec. Consider changing the client spec so that it does. Exiting.");
		return 1;
	}

	int networkThreads = 1;
	std::string networkThreadsStr = Arguments::GetSingleton()->GetNetworkThreads();
	if (!networkThreadsStr.empty())
	{
		networkThreads = std::atoi(networkThreadsStr.c_str());
	}

	int printBatch = 1;
	std::string printBatchStr = Arguments::GetSingleton()->GetPrintBatch();
	if (!printBatchStr.empty())
	{
		printBatch = std::atoi(printBatchStr.c_str());
	}

	int lookAhead = 1;
	std::string lookAheadStr = Arguments::GetSingleton()->GetLookAhead();
	if (!lookAheadStr.empty())
	{
		lookAhead = std::atoi(lookAheadStr.c_str());
	}

	std::string retriesStr = Arguments::GetSingleton()->GetRetries();
	if (!retriesStr.empty())
	{
		P4API::CommandRetries = std::atoi(retriesStr.c_str());
	}

	std::string refreshStr = Arguments::GetSingleton()->GetRefresh();
	if (!refreshStr.empty())
	{
		P4API::CommandRefreshThreshold = std::atoi(refreshStr.c_str());
	}

	std::vector<StreamResult::MappingData> mappings {};
	std::vector<StreamResult::MappingData> exclusions {};

	if (streamMappings)
	{
		PRINT("STREAM MAPPING MODE!");
		PRINT("NOTE: validate-migration.sh might say the stream is incorrect even if it complies to the stream spec");
		// p4 stream takes a path in the form "//depot/stream" as opposed to "//depot/stream/..."
		auto tempStr = depotPath.substr(0, depotPath.size() - 4);
		auto val = p4.Stream(tempStr);
		// First we get the shallow imports.
		for (auto const& v : val->GetStreamSpec().mapping)
		{
			// We don't need stream isolate or share, so we don't check them
			if (v.rule == StreamResult::EStreamImport)
			{
				mappings.push_back(v);
			}
			else if (v.rule == StreamResult::EStreamExclude)
			{
				exclusions.push_back(v);
			}
		}

		// Nested mappings get handled here.
		for (auto idx = 0; idx < mappings.size(); idx++)
		{
			PRINT("Mapping: " << mappings[idx]);
			// Check this is not for a specific file
			if (!STDHelpers::EndsWith(mappings[idx].stream2, "..."))
			{
				continue;
			}

			// It's a path or a stream
			// A path isn't nessecarily a fully qualified stream in itself but p4 stream works with arbitrary paths
			auto streamName = mappings[idx].stream2.substr(0, mappings[idx].stream2.size() - 4);
			auto subStream = p4.Stream(streamName);
			for (auto& v : subStream->GetStreamSpec().mapping)
			{
				// According to the perforce rules, exclude should not be propagated outside the parent stream
				// The only rules we need to care about are import and exclude.
				if (v.rule == StreamResult::EStreamExclude)
				{
					StreamResult::MappingData tmp { v };
					// Do path mangling here.
					tmp.stream1 = streamName + "/" + v.stream1;
					exclusions.push_back(tmp);
				}
				else if (v.rule == StreamResult::EStreamImport)
				{
					// Mangle the path so it's in respect to the super repo
					// keep the "/"
					auto fakePath = mappings[idx].stream1.substr(0, mappings[idx].stream1.size() - 3);
					StreamResult::MappingData tmp { v };
					tmp.stream1 = fakePath + tmp.stream1;
					mappings.push_back(tmp);
				}
			}
		}
	}

	BranchSet branchSet(git,
	    P4API::ClientSpec.mapping,
	    depotPath,
	    branchNames,
	    mappings,
	    exclusions,
	    includeBinaries,
	    mergeDeletes,
	    caseInsensitive,
	    excludes,
	    overrideToTextSpecs,
	    overrideToBinarySpecs);

	bool profiling = false;
#if MTR_ENABLED
	profiling = true;
#endif

	PRINT("Perforce Port: " << P4API::P4PORT);
	PRINT("Perforce User: " << P4API::P4USER);
	PRINT("Perforce Client: " << P4API::P4CLIENT);
	PRINT("Depot Path: " << depotPath);
	PRINT("Network Threads: " << networkThreads);
	PRINT("Print Batch: " << printBatch);
	PRINT("Look Ahead: " << lookAhead);
	PRINT("Max Retries: " << retriesStr);
	PRINT("Max Changes: " << maxChanges);
	PRINT("Merge Deletes: " << mergeDeletes);
	PRINT("Case Handling: " << caseHandling);
	PRINT("Refresh Threshold: " << refreshStr);
	PRINT("Fsync Enable: " << fsyncEnable);
	PRINT("Include Binaries: " << includeBinaries);
	PRINT("Profiling: " << profiling);
	PRINT("Profiling Flush Rate: " << flushRate);
	PRINT("No Colored Output: " << noColor);
	PRINT("Inspecting " << branchSet.Count() << " branches");
	PRINT("Stream Mapping " << streamMappings);
	PRINT("Repack Rate: " << repackRate);
	PRINT("Dry Run: " << dryRun);
	PRINT("Exclude Log: " << excludeLog);
	PRINT("Include Log: " << includeLog);
	if (streamMappings)
	{
		PRINT("Mapped in Streams: " << mappings.size());
		PRINT("Excluded paths: " << exclusions.size());
	}

	bool repositoryOpened = false;
	if (dryRun)
	{
		repositoryOpened = git.TryOpenRepository(srcPath);
		if (repositoryOpened)
		{
			SUCCESS("Dry run: opened existing Git repository at " << srcPath << " read-only");
		}
		else
		{
			WARN("Dry run: no Git repository at " << srcPath << ", processing changelists from the beginning");
		}
	}
	else
	{
		if (!git.InitializeRepository(srcPath))
		{
			ERR("Could not initialize Git repository. Exiting.");
			return 1;
		}
		repositoryOpened = true;
	}

	// Setup trace file generation
	// In dry run mode the source path may not exist and must not be written, so the trace goes next to the logs.
	const std::string tracePath = dryRun ? logFolderPath : srcPath;
	mtr_init((tracePath + (tracePath.back() == '/' ? "" : "/") + "trace.json").c_str());
	MTR_META_PROCESS_NAME("p4-fusion");
	MTR_META_THREAD_NAME("Main Thread");
	MTR_META_THREAD_SORT_INDEX(0);

	std::string resumeFromCL;
	if (repositoryOpened && git.IsHEADExists())
	{
		const std::string lastCommitDepotPath = git.GetDepotPathFromLastCommit();
		const std::string depotPathWithoutDots = depotPath.substr(0, depotPath.size() - 3);
		if (branchSet.HasMergeableBranch())
		{
			bool foundMatchingBranch = false;
			for (const Branch& branch : branchSet.GetBranches())
			{
				const std::string branchDepotPath = depotPathWithoutDots + branch.depotBranchPath;

				if (lastCommitDepotPath == branchDepotPath)
				{
					foundMatchingBranch = true;
					break;
				}
			}
			if (!foundMatchingBranch)
			{
				ERR("Git repository at " << srcPath << " was not initially cloned with any provided branches. Exiting.");
				return 1;
			}
		}
		else
		{
			if (lastCommitDepotPath != depotPathWithoutDots)
			{
				ERR("Git repository at " << srcPath << " was not initially cloned with depotPath = \"" << depotPath << "\". Exiting.");
				return 1;
			}
		}

		resumeFromCL = git.DetectLatestCL();
		WARN("Detected last CL committed as CL " << resumeFromCL);
	}

	PRINT("Requesting changelists to convert from the Perforce server");

	std::vector<ChangeList> changes;
	if (branchSet.HasMergeableBranch())
	{
		for (const Branch& branch : branchSet.GetBranches())
		{
			const std::string branchDepotPath = depotPath.substr(0, depotPath.size() - 3) + branch.depotBranchPath + "/...";

			std::vector<ChangeList> branchChanges = std::move(p4.Changes(branchDepotPath, resumeFromCL, maxChanges)->GetChanges());
			for (ChangeList& cl : branchChanges)
			{
				changes.push_back(std::move(cl));
			}
		}
		std::sort(changes.begin(), changes.end());
	}
	else
	{
		changes = std::move(p4.Changes(depotPath, resumeFromCL, maxChanges)->GetChanges());
	}

	if (streamMappings)
	{
		PRINT("Path: " << depotPath << " has " << changes.size() << " changes!");
		for (auto const& mapped : mappings)
		{
			std::vector<ChangeList> temp = std::move(p4.Changes(mapped.stream2, resumeFromCL, maxChanges)->GetChanges());
			PRINT("Path: " << mapped.stream2 << " has " << temp.size() << " changes!");
			changes.insert(changes.end(), std::make_move_iterator(temp.begin()), std::make_move_iterator(temp.end()));
		}
		std::sort(changes.begin(), changes.end());
		// Truncate excess changes
		if ((maxChanges > -1) && (changes.size() > maxChanges))
		{
			// We don't need to consider the case if this ever expands the vector.
			changes.resize(maxChanges);
		}
	}

	// Detect and log changelists that appear more than once across branches
	{
		std::set<std::string> uniqueChangeListNumbers;
		std::set<std::string> notUniqueChangeListNumbers;
		std::vector<size_t> markForRemove;
		for (size_t i = 0; i < changes.size(); ++i)
		{
			ChangeList& cl = changes.at(i);
			if (!uniqueChangeListNumbers.insert(cl.number).second)
			{
				notUniqueChangeListNumbers.insert(cl.number);
				markForRemove.push_back(i);
			}
		}
		if (!notUniqueChangeListNumbers.empty())
		{
			WARN("Changelists appear in more than one branch! These will appear in each branch.");
			for (const auto& cl : notUniqueChangeListNumbers)
				WARN("Duplicate changelist: " << cl);

			// Remove duplicates
			for (size_t i = markForRemove.size(); i > 0; --i)
				changes.erase(changes.begin() + markForRemove[i - 1]);
		}
	}

	// Return early if we have no work to do
	if (changes.empty())
	{
		SUCCESS("Repository is up to date. Exiting.");
		return 0;
	}

	// The changes are received in chronological order
	SUCCESS("Found " << changes.size() << " uncloned CLs starting from CL " << changes.front().number << " to CL " << changes.back().number);

	PRINT("Creating " << networkThreads << " network threads");
	ThreadPool::GetSingleton()->Initialize(networkThreads, lfsClient.get());
	SUCCESS("Created " << ThreadPool::GetSingleton()->GetThreadCount() << " threads in thread pool");

	// Go in the chronological order
	size_t lastDownloadedCL = 0;
	for (size_t currentCL = 0; currentCL < changes.size() && currentCL < lookAhead; currentCL++)
	{
		ChangeList& cl = changes.at(currentCL);

		// Start gathering changed files with `p4 describe` or `p4 filelog`
		cl.PrepareDownload(branchSet);

		lastDownloadedCL = currentCL;
	}

	// This is intentionally put in a separate loop.
	// We want to submit `p4 describe` commands before sending any of the `p4 print` commands.
	// Gives ~15% perf boost.
	int startupDownloadsCount = 0;
	if (!dryRun)
	{
		for (size_t currentCL = 0; currentCL <= lastDownloadedCL; currentCL++)
		{
			ChangeList& cl = changes.at(currentCL);

			// Start running `p4 print` on changed files when the describe is finished. Upload LFS files if needed.
			cl.StartDownloadAndLFSUpload(printBatch);
			startupDownloadsCount++;
		}

		git.CreateIndex(lfsClient.get());
		SUCCESS("Queued first " << startupDownloadsCount << " CLs up until CL " << changes.at(lastDownloadedCL).number << " for downloading");
	}
	else
	{
		SUCCESS("Dry run: queued first " << lastDownloadedCL + 1 << " CLs up until CL " << changes.at(lastDownloadedCL).number << " for describing");
	}

	int timezoneMinutes = 0;
	std::unordered_map<UsersResult::UserID, UsersResult::UserData> users;
	if (!dryRun)
	{
		timezoneMinutes = p4.Info()->GetServerTimezoneMinutes();
		SUCCESS("Perforce server timezone is " << timezoneMinutes << " minutes");

		// Map usernames to emails
		users = std::move(p4.Users()->GetUserEmails());
		SUCCESS("Received userbase details from the Perforce server");
	}

	// Commit procedure start
	Timer commitTimer;
	size_t includedFileCount = 0;
	size_t lfsFileCount = 0;
	size_t excludedFileCount = 0;

	PRINT("Last CL to start " << (dryRun ? "describing" : "downloading") << " is CL " << changes.at(lastDownloadedCL).number);

	for (size_t i = 0; i < changes.size(); i++)
	{
		// See if the threadpool encountered any exceptions
		try
		{
			ThreadPool::GetSingleton()->RaiseCaughtExceptions();
		}
		catch (const std::exception& e)
		{
			// This is unrecoverable
			ERR("Threadpool encountered an exception: " << e.what());
			ThreadPool::GetSingleton()->ShutDown();
			std::exit(1);
		}

		ChangeList& cl = changes.at(i);

		if (dryRun)
		{
			cl.WaitForBeingDescribed();
		}
		else
		{
			cl.WaitForBeingCommitReady();
		}

		const auto& excludedFiles = cl.changedFileGroups->excludedFiles;
		if (excludeLogStream && !excludedFiles.empty())
		{
			for (const auto& file : excludedFiles)
			{
				*excludeLogStream << cl.number << ' ' << file << '\n';
			}
			excludeLogStream->flush();

			if (excludeLogStream->fail())
			{
				ERR("Failed to write exclude log file: " << excludeLogPath << " (CL " << cl.number << "). Exiting.");
				ThreadPool::GetSingleton()->ShutDown();
				std::exit(1);
			}
		}

		if (includeLogStream)
		{
			for (const auto& branchGroup : cl.changedFileGroups->branchedFileGroups)
			{
				const std::string branchName = branchGroup.targetBranch.empty() ? "-" : branchGroup.targetBranch;
				for (const auto& file : branchGroup.files)
				{
					// Same classification as in ChangeList::DownloadBatch, so the log matches what a real run commits.
					const char* kind = "git";
					if (file.IsDeleted())
					{
						kind = "del";
					}
					else if (lfsClient && lfsClient->IsLFSTracked(P4Unescape(file.GetRelativeDepotPath())))
					{
						kind = "lfs";
						lfsFileCount++;
					}

					// Keep the line space separated: some synthetic action names contain spaces.
					std::string actionName = file.GetActionName();
					std::replace(actionName.begin(), actionName.end(), ' ', '_');

					*includeLogStream << cl.number << ' ' << kind << ' ' << branchName << ' ' << actionName << ' '
					                  << file.GetDepotFile() << '#' << file.GetRevision() << '\n';
					includedFileCount++;
				}
			}
			includeLogStream->flush();

			if (includeLogStream->fail())
			{
				ERR("Failed to write include log file: " << includeLogPath << " (CL " << cl.number << "). Exiting.");
				ThreadPool::GetSingleton()->ShutDown();
				std::exit(1);
			}
		}

		if (dryRun)
		{
			excludedFileCount += excludedFiles.size();

			for (const auto& branchGroup : cl.changedFileGroups->branchedFileGroups)
			{
				SUCCESS(
				    "[DRY RUN] CL " << cl.number << " would commit "
				                    << branchGroup.files.size() << " files"
				                    << (branchGroup.targetBranch.empty()
				                               ? ""
				                               : (" to branch " + branchGroup.targetBranch))
				                    << (branchGroup.hasSource && !noMerge
				                               ? (" merged from branch " + branchGroup.sourceBranch)
				                               : "")
				                    << ".");
			}
			SUCCESS(
			    "[DRY RUN] CL " << cl.number << " with "
			                    << cl.changedFileGroups->totalFileCount << " files, "
			                    << excludedFiles.size() << " excluded (" << i + 1 << "/" << changes.size()
			                    << "). Elapsed " << commitTimer.GetTimeS() / 60.0f << " mins.");

			cl.Clear();

			if (lastDownloadedCL + 1 < changes.size())
			{
				lastDownloadedCL++;
				changes.at(lastDownloadedCL).PrepareDownload(branchSet);
			}

			if ((i % flushRate) == 0)
			{
				mtr_flush();
			}

			continue;
		}

		std::string fullName = cl.user;
		std::string email = "deleted@user";
		if (users.find(cl.user) != users.end())
		{
			fullName = users.at(cl.user).fullName;
			email = users.at(cl.user).email;
		}

		for (auto& branchGroup : cl.changedFileGroups->branchedFileGroups)
		{
			if (!branchGroup.targetBranch.empty())
			{
				git.SetActiveBranch(branchGroup.targetBranch);
			}

			for (auto& file : branchGroup.files)
			{
				std::string unescapedPath = P4Unescape(file.GetRelativeDepotPath());
				if (file.IsDeleted())
				{
					git.RemoveFileFromIndex(unescapedPath);
				}
				else
				{
					git.AddFileToIndex(unescapedPath, file.GetContents(), file.IsExecutable());
				}

				// No use for keeping the contents in memory once it has been added
				file.Clear();
			}

			std::string mergeFrom = "";
			if (branchGroup.hasSource && !noMerge)
			{
				// Only perform merging if the branch group explicitly declares that the change
				// has a source, and if the user wants merging.
				mergeFrom = branchGroup.sourceBranch;
			}

			std::string depotPathString = depotPath.substr(0, depotPath.size() - 3);
			if (branchSet.HasMergeableBranch())
			{
				depotPathString += branchGroup.depotBranchPath;
			}

			PRINT_VERBOSE("Running git commit based on " << cl.number << " into " << branchGroup.targetBranch << " branch");

			std::string commitSHA = git.Commit(depotPathString,
			    cl.number,
			    fullName,
			    email,
			    timezoneMinutes,
			    cl.description,
			    cl.timestamp,
			    mergeFrom);

			PRINT_VERBOSE("Done git command");

			// For scripting/testing purposes...
			SUCCESS(
			    "CL " << cl.number << " --> Commit " << commitSHA
			          << " with " << branchGroup.files.size() << " files"
			          << (branchGroup.targetBranch.empty()
			                     ? ""
			                     : (" to branch " + branchGroup.targetBranch))
			          << (branchGroup.sourceBranch.empty()
			                     ? ""
			                     : (" from branch " + branchGroup.sourceBranch))
			          << ".");
		}
		SUCCESS(
		    "CL " << cl.number << " with "
		          << cl.changedFileGroups->totalFileCount << " files (" << i + 1 << "/" << changes.size()
		          << "|" << lastDownloadedCL - (long long)i
		          << "). Elapsed " << commitTimer.GetTimeS() / 60.0f << " mins. (Estimated "
		          << ((commitTimer.GetTimeS() / 60.0f) / (float)(i + 1)) * (changes.size() - i - 1) << " mins left.)");

		// Clear out finished changelist.
		cl.Clear();

		bool repackNeeded = (repackRate > 0) && (((i + 1) % repackRate == 0) || (i == (changes.size() - 1)));
		if (repackNeeded)
		{
			PRINT_VERBOSE("Performing incremental git repack.");
			bool incremental = true;
			if (git.RepackWithSystemCommand(srcPath, incremental))
			{
				PRINT_VERBOSE("Git repack command succeeded.");
			}
			else
			{
				WARN("Git repack command failed, but continuing conversion.");
			}
		}

		// Start downloading the CL chronologically after the last CL that was previously downloaded, if there's still some left
		if (lastDownloadedCL + 1 < changes.size())
		{
			lastDownloadedCL++;
			ChangeList& downloadCL = changes.at(lastDownloadedCL);
			downloadCL.PrepareDownload(branchSet);
			downloadCL.StartDownloadAndLFSUpload(printBatch);
		}

		// Occasionally flush the profiling data
		if ((i % flushRate) == 0)
		{
			mtr_flush();
		}

		// Deallocate this CL's metadata from memory
		cl.Clear();
	}
	if (dryRun)
	{
		SUCCESS("[DRY RUN] Completed processing of " << changes.size() << " CLs in " << programTimer.GetTimeS() / 60.0f << " minutes: "
		                                             << includedFileCount << " files would be committed (" << lfsFileCount << " to LFS), "
		                                             << excludedFileCount << " files excluded. No files were downloaded and the git repository was not modified.");
	}
	else
	{
		git.CloseIndex();

		SUCCESS("Completed conversion of " << changes.size() << " CLs in " << programTimer.GetTimeS() / 60.0f << " minutes, taking " << commitTimer.GetTimeS() / 60.0f << " to commit CLs");
	}

	ThreadPool::GetSingleton()->ShutDown();

	if (!P4API::ShutdownLibraries())
	{
		return 1;
	}

	mtr_flush();
	mtr_shutdown();

	return 0;
}

void SignalHandler(sig_atomic_t s)
{
	static bool called = false;
	if (called)
	{
		WARN("Already received an interrupt signal, waiting for threads to shutdown.");
		return;
	}
	called = true;

	ERR("Signal Received: " << strsignal(s));

	ThreadPool::GetSingleton()->ShutDown();

	std::exit(s);
}

int main(int argc, char** argv)
{
	int exitCode = 0;

	Aws::SDKOptions awsOptions;
	Aws::InitAPI(awsOptions);

	try
	{
		exitCode = Main(argc, argv);
	}
	catch (const std::exception& e)
	{
		ERR("Exception occurred: " << typeid(e).name() << ": " << e.what());
		return 1;
	}

	Aws::ShutdownAPI(awsOptions);
	return exitCode;
}
