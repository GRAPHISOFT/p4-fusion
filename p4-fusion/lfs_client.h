/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <string>
#include <vector>

#include "git2/pathspec.h"

#include "git_api.h"

// Thread-safe class for handling Git LFS (Large File Storage) operations
class LFSClient
{
private:
	enum class API
	{
		LFS,
		S3
	};

public:
	enum class UploadResult
	{
		Uploaded,
		AlreadyExists,
		UnknownAPI,
		Error
	};

	/**
	 * Constructor for the LFSClient using LFS server API.
	 */
	LFSClient(GitAPI& gitAPI, const std::string& serverUrl, const std::string& username, const std::string& password, const std::vector<std::string>& lfsPatterns);

	/**
	 * Constructor for the LFSClient using S3 API.
	 */
	LFSClient(GitAPI& gitAPI, const std::string& serverUrl, const std::string& bucket, const std::string& repository, const std::string& username, const std::string& password, const std::vector<std::string>& lfsPatterns);

	std::vector<char> CreatePointerFileContents(const std::vector<char>& fileContents) const;

	static std::string CalcOID(const std::vector<char>& fileContents);
	UploadResult UploadFile(const std::vector<char>& fileContents) const;

	bool IsLFSTracked(const std::string& filePath) const;

	std::vector<char> GetGitAttributesContents() const;

	const std::string& GetServerUrl() const { return m_ServerUrl; }
	const std::string& GetUsername() const { return m_Username; }
	const std::string& GetPassword() const { return m_Password; }

private:
	GitAPI& m_GitAPI;
	const API m_LFSAPI;

	const std::string m_ServerUrl;
	const std::string m_Bucket; // Only used for S3 API
	const std::string m_Repository; // Only used for S3 API
	const std::string m_Username;
	const std::string m_Password;

	std::vector<std::string> m_LFSPatterns;
	UniqueGitPathSpec m_LFSPathSpec;
};