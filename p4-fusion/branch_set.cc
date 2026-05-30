/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "branch_set.h"
#include <algorithm>
#include <map>

static const std::string EMPTY_STRING = "";
static const std::array<std::string, 2> INVALID_BRANCH_PATH { EMPTY_STRING, EMPTY_STRING };

ChangedFileGroups::ChangedFileGroups()
    : totalFileCount(0)
{
}

ChangedFileGroups::ChangedFileGroups(std::vector<BranchedFileGroup>& groups, int totalFileCount)
    : totalFileCount(totalFileCount)
{
	branchedFileGroups = std::move(groups);
}

void ChangedFileGroups::Clear()
{
	for (auto& fileGroup : branchedFileGroups)
	{
		for (auto& file : fileGroup.files)
		{
			file.Clear();
		}
		fileGroup.files.clear();
		fileGroup.sourceBranch.clear();
		fileGroup.targetBranch.clear();
	}
	totalFileCount = 0;
}

Branch::Branch(const std::string& branch, const std::string& alias)
    : depotBranchPath(branch)
    , gitAlias(alias)
{
	if (depotBranchPath.empty())
	{
		throw std::invalid_argument("branch name is empty");
	}
	if (gitAlias.empty())
	{
		throw std::invalid_argument("branch alias is empty");
	}
}

bool Branch::IsInBranch(const std::string& relativeDepotPath) const
{
	return
	    // The relative depot branch, to match this branch path, must start with the
	    // branch path + "/".  The "StartsWith" is put at the end of the 'and' checks,
	    // because it takes the longest.
	    relativeDepotPath.size() > depotBranchPath.size()
	    && relativeDepotPath[depotBranchPath.size()] == '/'
	    && STDHelpers::StartsWith(relativeDepotPath, depotBranchPath);
}

Branch createBranchFromPath(const std::string& depotBranchPath)
{
	std::string branchPath = std::string(depotBranchPath);
	std::string alias = std::string(depotBranchPath);

	// The formatting using a ':' to separate the branch path from the git alias MUST be
	// the last ':' in the string.  This allows the command to work with branch paths that contain
	// a ':' character, as long as the git alias does NOT contain a ':', and it implies that the git
	// alias MUST be given.
	size_t pos = depotBranchPath.rfind(':');
	if (pos > 0 && depotBranchPath.size() > pos)
	{
		branchPath.erase(pos);
		alias.erase(0, pos + 1);
	}

	STDHelpers::StripSurrounding(branchPath, '/');
	STDHelpers::StripSurrounding(alias, '/');
	return Branch(branchPath, alias);
}

std::vector<Branch> createBranchesFromPaths(const std::vector<std::string>& branches)
{
	std::vector<Branch> parsed;
	for (auto& branch : branches)
	{
		parsed.push_back(createBranchFromPath(branch));
	}
	return parsed;
}

BranchSet::BranchSet(GitAPI& gitAPI,
    const std::vector<std::string>& clientViewMapping,
    const std::string& baseDepotPath,
    const std::vector<std::string>& branches,
    const std::vector<StreamResult::MappingData>& mappings,
    const std::vector<StreamResult::MappingData>& exclusions,
    const bool includeBinaries,
	const bool mergeDeletes,
    const std::vector<std::regex>& excludes,
    const std::vector<std::string>& overrideToTextSpecs,
    const std::vector<std::string>& overrideToBinarySpecs)
    : m_gitApi(gitAPI)
    , m_branches(createBranchesFromPaths(branches))
    , m_mappings(mappings)
    , m_exclusions(exclusions)
    , m_includeBinaries(includeBinaries)
	, m_mergeDeletes(mergeDeletes)
    , m_excludes(excludes)
    , m_overrideToTextSpec(nullptr, nullptr)
    , m_overrideToBinarySpec(nullptr, nullptr)
{
	m_view.InsertTranslationMapping(clientViewMapping);
	if (STDHelpers::EndsWith(baseDepotPath, "/..."))
	{
		// Keep the final '/'.
		m_basePath = baseDepotPath.substr(0, baseDepotPath.size() - 3);
	}
	else if (baseDepotPath.back() != '/')
	{
		throw std::invalid_argument("Bad base depot path format: " + baseDepotPath);
	}
	else
	{
		m_basePath = baseDepotPath;
	}

	if (!overrideToTextSpecs.empty())
	{
		m_overrideToTextSpec = m_gitApi.CreatePathSpec(overrideToTextSpecs);
	}
	if (!overrideToBinarySpecs.empty())
	{
		m_overrideToBinarySpec = m_gitApi.CreatePathSpec(overrideToBinarySpecs);
	}
}

const Branch* BranchSet::getBranchFor(const std::string& relativeDepotPath) const
{
	// Check if the relative depot path starts with any of the branches.
	// This checks the branches in their stored order, which can mean that having a branch
	// order like "//a/b/c" and "//a/b" will only work if the sub-branches are listed first.
	// To do this properly, the stored branches should be scanned based on their length - longest
	// first, but that's extra processing and code for a use case that is rare and has a manual
	// work around (list branches in a specific order).
	for (const Branch& branch : m_branches)
	{
		if (branch.IsInBranch(relativeDepotPath))
		{
			return &branch;
		}
	}
	return nullptr;
}

bool BranchSet::matchesExcludes(const std::string& depotPath) const
{
	for (const auto& exclude : m_excludes)
	{
		if (std::regex_match(depotPath, exclude))
		{
			return true;
		}
	}
	return false;
}

std::string BranchSet::stripBasePath(const std::string& depotPath) const
{
	if (STDHelpers::StartsWith(depotPath, m_basePath))
	{
		// strip off the leading '/', too.
		return depotPath.substr(m_basePath.size());
	}
	return EMPTY_STRING;
}

struct branchIntegrationMap
{
	std::vector<BranchedFileGroup> branchGroups;
	std::unordered_map<std::string, int> branchIndices;
	int fileCount = 0;

	void addMerge(const std::string& sourceBranch, const std::string& targetBranch, const std::string& depotBranchPath, const FileData& rev);
	void addTarget(const std::string& targetBranch, const std::string& depotBranchPath, const FileData& rev, bool mergeEligible);
	static std::string createTargetOnlyMapKey(const std::string& targetBranch, bool mergeEligible);

	void consolidateTargetGroups();
	static bool isMergeEligibleTargetFile(const FileData& fileData);

	// note: not const, because it cleans out the branchGroups.
	std::unique_ptr<ChangedFileGroups> createChangedFileGroups(bool mergeDeletes)
	{
		if (mergeDeletes)
		{
			consolidateTargetGroups();
		}
		return std::unique_ptr<ChangedFileGroups>(new ChangedFileGroups(branchGroups, fileCount));
	};
};

std::string branchIntegrationMap::createTargetOnlyMapKey(const std::string& targetBranch, bool mergeEligible)
{
	return EMPTY_STRING + std::string("/") + targetBranch + (mergeEligible ? "/merge-eligible" : "/target-only");
}

void branchIntegrationMap::addTarget(const std::string& targetBranch, const std::string& depotBranchPath, const FileData& fileData, bool mergeEligible)
{
	const std::string mapKey = createTargetOnlyMapKey(targetBranch, mergeEligible);
	const auto entry = branchIndices.find(mapKey);
	if (entry == branchIndices.end())
	{
		const int index = branchGroups.size();
		branchIndices.insert(std::make_pair(mapKey, index));
		branchGroups.push_back(BranchedFileGroup());
		BranchedFileGroup& bfg = branchGroups[index];
		bfg.sourceBranch = EMPTY_STRING;
		bfg.targetBranch = targetBranch;
		bfg.depotBranchPath = depotBranchPath;
		bfg.hasSource = false;
		bfg.files.push_back(fileData);
	}
	else
	{
		branchGroups.at(entry->second).files.push_back(fileData);
	}
	fileCount++;
}

void branchIntegrationMap::addMerge(const std::string& sourceBranch, const std::string& targetBranch, const std::string& depotBranchPath, const FileData& fileData)
{
	// Need to store this in the integration map, using "src/tgt" as the
	// key.  Because stream names can't have a '/' in them, this creates a unique key.
	// source might be empty, and that's okay.
	const std::string mapKey = sourceBranch + "/" + targetBranch;
	const auto entry = branchIndices.find(mapKey);
	if (entry == branchIndices.end())
	{
		const int index = branchGroups.size();
		branchIndices.insert(std::make_pair(mapKey, index));
		branchGroups.push_back(BranchedFileGroup());
		BranchedFileGroup& bfg = branchGroups[index];
		bfg.sourceBranch = sourceBranch;
		bfg.targetBranch = targetBranch;
		bfg.depotBranchPath = depotBranchPath;
		bfg.hasSource = !sourceBranch.empty();
		bfg.files.push_back(fileData);
	}
	else
	{
		branchGroups.at(entry->second).files.push_back(fileData);
	}
	fileCount++;
}

bool branchIntegrationMap::isMergeEligibleTargetFile(const FileData& fileData)
{
	switch (fileData.GetAction())
	{
	case FileAction::FileIntegrateDelete:
	case FileAction::FileMoveAdd:
	case FileAction::FileMoveDelete:
		return true;
	default:
		return false;
	}
}

void branchIntegrationMap::consolidateTargetGroups()
{
	// Fold target-only groups (e.g. move/deletes on the target side of an integration)
	// into a merge group that shares the same target branch.  This ensures these files
	// appear in the merge commit rather than a separate non-merge commit.
	//
	// Only consolidate when exactly one merge group exists for a given target branch.
	// If multiple merge groups exist (integrations from different source branches),
	// the target-only files are left as their own commit to avoid associating them
	// with an arbitrary merge parent.

	// First pass: count merge groups per target branch and record their index.
	std::unordered_map<std::string, std::vector<size_t>> targetMergeGroups;
	for (size_t i = 0; i < branchGroups.size(); i++)
	{
		BranchedFileGroup& group = branchGroups[i];
		if (group.hasSource && !group.targetBranch.empty())
		{
			targetMergeGroups[group.targetBranch].push_back(i);
		}
	}

	// Second pass: fold target-only groups into merge groups where unambiguous.
	std::vector<size_t> groupsToRemove;
	for (size_t i = 0; i < branchGroups.size(); i++)
	{
		BranchedFileGroup& group = branchGroups[i];
		if (group.hasSource || group.targetBranch.empty())
		{
			continue;
		}
		if (!std::all_of(group.files.begin(), group.files.end(), isMergeEligibleTargetFile))
		{
			continue;
		}
		// This is a target-only group.  Only fold if exactly one merge group
		// exists for this target branch, or if files can be mapped unambiguously
		// to one of multiple merge groups.
		auto it = targetMergeGroups.find(group.targetBranch);
		if (it != targetMergeGroups.end() && it->second.size() == 1)
		{
			BranchedFileGroup& mergeGroup = branchGroups[it->second[0]];
			for (auto& file : group.files)
			{
				mergeGroup.files.push_back(file);
			}
			group.files.clear();
			groupsToRemove.push_back(i);
		}
		else if (it != targetMergeGroups.end() && it->second.size() > 1)
		{
			auto getFileName = [](const std::string& path) -> std::string
			{
				size_t slashPos = path.find_last_of('/');
				return slashPos == std::string::npos ? path : path.substr(slashPos + 1);
			};

			// Attempt to map each target-only file to a specific merge group by
			// first using the target file's own fromDepot value (if present), then
			// matching target relative path against source fromDepot paths.
			std::unordered_map<size_t, std::unordered_set<std::string>> mergeFromDepotPaths;
			std::unordered_map<size_t, std::vector<std::string>> mergeSourcePrefixes;
			std::unordered_map<size_t, std::unordered_set<std::string>> mergeFromDepotFileNames;
			std::unordered_map<size_t, std::unordered_set<std::string>> mergeMovedFromDepotPaths;

			for (size_t mergeGroupIndex : it->second)
			{
				const BranchedFileGroup& mergeGroup = branchGroups[mergeGroupIndex];
				for (const auto& mergeFile : mergeGroup.files)
				{
					const std::string& movedFromDepot = mergeFile.GetMovedFromDepotFile();
					if (!movedFromDepot.empty())
					{
						mergeMovedFromDepotPaths[mergeGroupIndex].insert(movedFromDepot);
					}

					const std::string& fromDepot = mergeFile.GetFromDepotFile();
					if (fromDepot.empty())
					{
						continue;
					}

					mergeFromDepotPaths[mergeGroupIndex].insert(fromDepot);
					mergeFromDepotFileNames[mergeGroupIndex].insert(getFileName(fromDepot));

					const std::string& mergeRelativePath = mergeFile.GetRelativeDepotPath();
					if (!mergeRelativePath.empty()
					    && fromDepot.size() > mergeRelativePath.size()
					    && STDHelpers::EndsWith(fromDepot, mergeRelativePath))
					{
						mergeSourcePrefixes[mergeGroupIndex].push_back(fromDepot.substr(0, fromDepot.size() - mergeRelativePath.size()));
					}
				}
			}

			std::vector<FileData> unassigned;
			for (auto& targetFile : group.files)
			{
				const std::string& targetRelativePath = targetFile.GetRelativeDepotPath();
				const std::string& targetFromDepot = targetFile.GetFromDepotFile();
				const std::string targetFileName = getFileName(targetRelativePath);
				std::vector<size_t> matchingMergeGroups;

				for (size_t mergeGroupIndex : it->second)
				{
					bool isMatch = false;

					const auto& fromDepotSet = mergeFromDepotPaths[mergeGroupIndex];

					// Strongest signal: target file has an explicit integration source.
					if (!targetFromDepot.empty())
					{
						if (fromDepotSet.find(targetFromDepot) != fromDepotSet.end())
						{
							isMatch = true;
						}
						else
						{
							const auto& sourcePrefixes = mergeSourcePrefixes[mergeGroupIndex];
							for (const std::string& sourcePrefix : sourcePrefixes)
							{
								if (STDHelpers::StartsWith(targetFromDepot, sourcePrefix))
								{
									isMatch = true;
									break;
								}
							}
						}
					}

					for (const std::string& fromDepot : fromDepotSet)
					{
						if (isMatch)
						{
							break;
						}
						if (STDHelpers::EndsWith(fromDepot, std::string("/") + targetRelativePath))
						{
							isMatch = true;
							break;
						}
					}

					if (!isMatch)
					{
						const auto& sourcePrefixes = mergeSourcePrefixes[mergeGroupIndex];
						for (const std::string& sourcePrefix : sourcePrefixes)
						{
							if (fromDepotSet.find(sourcePrefix + targetRelativePath) != fromDepotSet.end())
							{
								isMatch = true;
								break;
							}
						}
					}

					if (!isMatch)
					{
						const auto& movedFromPaths = mergeMovedFromDepotPaths[mergeGroupIndex];
						if (movedFromPaths.find(targetFile.GetDepotFile()) != movedFromPaths.end())
						{
							isMatch = true;
						}
					}

					if (!isMatch && !targetFileName.empty())
					{
						const auto& sourceFileNames = mergeFromDepotFileNames[mergeGroupIndex];
						if (sourceFileNames.find(targetFileName) != sourceFileNames.end())
						{
							isMatch = true;
						}
					}

					if (isMatch)
					{
						matchingMergeGroups.push_back(mergeGroupIndex);
					}
				}

				if (matchingMergeGroups.size() == 1)
				{
					branchGroups[matchingMergeGroups[0]].files.push_back(targetFile);
				}
				else
				{
					unassigned.push_back(targetFile);
				}
			}

			group.files = std::move(unassigned);
			if (group.files.empty())
			{
				groupsToRemove.push_back(i);
			}
		}
	}

	// Remove folded groups in reverse order to preserve indices.
	for (auto it = groupsToRemove.rbegin(); it != groupsToRemove.rend(); ++it)
	{
		branchGroups.erase(branchGroups.begin() + *it);
	}
}

// Post condition: all returned FileData (e.g. filtered for git commit) have the relativePath set.
std::unique_ptr<ChangedFileGroups> BranchSet::ParseAffectedFiles(const std::vector<FileData>& cl, const LFSClient* lfsClient) const
{
	branchIntegrationMap branchMap;
	for (auto& clFileData : cl)
	{
		FileData fileData(clFileData);

		// First, filter out files we don't want.
		const std::string& depotFile = fileData.GetDepotFile();
		std::string relativeDepotPath = stripBasePath(depotFile);
		const bool matchesAnyExcludes = matchesExcludes(depotFile);
		if (matchesAnyExcludes)
		{
			const std::string depotBasePath = depotFile.substr(0, depotFile.find_last_of('/'));
			m_excludedFileDirs.insert(depotBasePath);
		}

		const bool isP4Binary = fileData.IsBinary();
		const bool isLFSTracked = lfsClient && lfsClient->IsLFSTracked(relativeDepotPath);
		const bool overrideToBinary = m_overrideToBinarySpec && git_pathspec_matches_path(m_overrideToBinarySpec.get(), GIT_PATHSPEC_IGNORE_CASE, relativeDepotPath.c_str());
		const bool overrideToText = m_overrideToTextSpec && git_pathspec_matches_path(m_overrideToTextSpec.get(), GIT_PATHSPEC_IGNORE_CASE, relativeDepotPath.c_str());

		if (
		    // depot file should always be present.
		    // The left side of the client view is the depot side.
		    !m_view.IsInLeft(depotFile)
		    || (!m_includeBinaries && ((isP4Binary && !overrideToText) || overrideToBinary))
		    || STDHelpers::Contains(depotFile, "/.git/") // To avoid adding .git/ files in the Perforce history if any
		    || STDHelpers::EndsWith(depotFile, "/.git") // To avoid adding a .git submodule file in the Perforce history if any
		    || matchesAnyExcludes // Exclude files that match any excludes regexes
		)
		{
			continue;
		}

		// Put logic for Absolutely do not dare map these files here, here :)
		if (relativeDepotPath.empty())
		{
			// Not under regular depot path, might be mapped in
			// check and attenuate if it is.
			bool isImport = false;
			for (auto const& v : m_mappings)
			{
				if (STDHelpers::EndsWith(v.stream2, "..."))
				{
					// get rid of trailing ...
					auto tempStr = v.stream2.substr(0, v.stream2.size() - 3);
					if (STDHelpers::StartsWith(depotFile, tempStr))
					{
						isImport = true;
						// Shove the replacement path at the front
						relativeDepotPath = v.stream1.substr(0, v.stream1.size() - 3) + depotFile.substr(tempStr.size());
						break;
					}
					if (depotFile == tempStr)
					{
						// Map in exactly one file and nothing else.
						relativeDepotPath = v.stream1;
						isImport = true;
						break;
					}
				}
			}

			if (!isImport)
			{
				continue;
			}

			if (isP4Binary && !overrideToText && m_includeBinaries && lfsClient && !isLFSTracked)
			{
				WARN("File " << depotFile << " at revision " << fileData.GetRevision() << " has filetype binary, but is not LFS tracked. It will be committed directly to the git repo.");
			}
		}

		// Check the file or path is not marked as excluded (There is a chance that not all of a mapped in directory is desired, so we have to check post mapping.)
		bool discard = false;
		for (auto const& v : m_exclusions)
		{
			if (STDHelpers::EndsWith(v.stream1, "..."))
			{
				if (STDHelpers::StartsWith(relativeDepotPath, v.stream1.substr(0, v.stream1.size() - 3)))
				{
					discard = true;
					break;
				}
			}
			else if (v.stream1 == relativeDepotPath)
			{
				discard = true;
				break;
			}
		}
		if (discard)
		{
			continue;
		}

		// If we have branches, then possibly sort the file into a branch group.
		if (HasMergeableBranch())
		{
			// [0] == branch name, [1] == relative path in the branch.
			const Branch* branch = getBranchFor(relativeDepotPath);
			if (branch == nullptr)
			{
				// not a valid branch file.  skip it.
				continue;
			}

			// It's a valid destination to a branch.
			// Make sure the relative path is set.
			const std::string branchFilePath = relativeDepotPath.substr(branch->depotBranchPath.size() + 1);
			fileData.SetRelativeDepotPath(branchFilePath);

			bool needsHandling = true;
			if (fileData.IsIntegrated())
			{
				// Only add the integration if the source is from a branch we care about.
				// [0] == branch name, [1] == relative path in the branch.
				const Branch* fromBranch = getBranchFor(stripBasePath(fileData.GetFromDepotFile()));
				if (fromBranch != nullptr
				    // Can't have source and target be pointing to the same branch; that's not
				    // a branch operation in the Git sense.
				    && fromBranch->gitAlias != branch->gitAlias)
				{
					// This is a valid integrate from a known source to a known target branch.
					branchMap.addMerge(fromBranch->gitAlias, branch->gitAlias, branch->depotBranchPath, fileData);
					needsHandling = false;
				}
			}
			if (needsHandling)
			{
				// Either not a valid integrate, or a normal operation.
				const bool isMergeEligible = m_mergeDeletes && branchIntegrationMap::isMergeEligibleTargetFile(fileData);
				branchMap.addTarget(branch->gitAlias, branch->depotBranchPath, fileData, isMergeEligible);
			}
		}
		else
		{
			// It's a non-branching setup.
			// Make sure the relative path is set.
			fileData.SetRelativeDepotPath(relativeDepotPath);
			branchMap.addTarget(EMPTY_STRING, EMPTY_STRING, fileData, false);
		}
	}
	return branchMap.createChangedFileGroups(m_mergeDeletes);
}
