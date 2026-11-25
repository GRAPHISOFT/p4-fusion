/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>
#include <vector>
#include <utility>

#include "common.h"

#include "git2/oid.h"
#include "git2/pathspec.h"

struct git_repository;
class LFSClient;

using UniqueGitPathSpec = std::unique_ptr<git_pathspec, decltype(&git_pathspec_free)>;

class GitAPI
{
	git_repository* m_Repo = nullptr;
	git_index* m_Index = nullptr;
	git_oid m_FirstCommitOid;

	std::string m_CurrentBranch = "";

	bool FindDirectoryIgnoreCase(const std::string& parentPath, const std::string& directoryName, std::string& existingName);

public:
	GitAPI(bool fsyncEnable);
	~GitAPI();

	bool InitializeRepository(const std::string& srcPath);
	void OpenRepository(const std::string& repoPath);

	bool IsHEADExists();
	bool IsBranchExists(const std::string& branchName);
	std::string GetDepotPathFromLastCommit() const;
	std::string DetectLatestCL();

	git_oid CreateBlob(const std::vector<char>& data);

	UniqueGitPathSpec CreatePathSpec(const std::vector<std::string>& patterns);

	void CreateIndex(LFSClient* lfsClient);
	void SetActiveBranch(const std::string& branchName);
	void AddFileToIndex(const std::string& relativePath, const std::vector<char>& contents, const bool plusx);
	void RemoveFileFromIndex(const std::string& relativePath);

	/// Replaces the case of each directory in the path with the case of an already existing directory
	/// in the index (the active branch's current state), if one exists with a different case.
	/// The file name and directories not existing yet in any case are kept as they are.
	std::string CanonicalizeDirectoryCase(const std::string& relativePath);

	bool RepackWithSystemCommand(const std::string& repoPath, bool incrementalRepack);

	std::string Commit(
	    const std::string& depotPath,
	    const std::string& cl,
	    const std::string& user,
	    const std::string& email,
	    const int& timezone,
	    std::string desc,
	    const int64_t& timestamp,
	    const std::string& mergeFromStream);
	void CloseIndex();
};
