/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "lfs_client.h"
#include "openssl/sha.h"
#include "lfscommunication/communicator.h"

LFSClient::LFSClient(GitAPI& gitAPI, const std::string& serverUrl, const std::string& username, const std::string& password, const std::vector<std::string>& lfsPatterns)
    : m_GitAPI(gitAPI)
    , m_LFSAPI(API::LFS)
    , m_ServerUrl(serverUrl)
    , m_Username(username)
    , m_Password(password)
    , m_LFSPatterns(lfsPatterns)
    , m_LFSPathSpec(m_GitAPI.CreatePathSpec(lfsPatterns))
{
}

LFSClient::LFSClient(GitAPI& gitAPI, const std::string& serverUrl, const std::string& bucket, const std::string& repository, const std::string& username, const std::string& password, const std::vector<std::string>& lfsPatterns)
	: m_GitAPI(gitAPI)
	, m_LFSAPI(API::S3)
	, m_ServerUrl(serverUrl)
	, m_Bucket(bucket)
	, m_Repository(repository)
	, m_Username(username)
	, m_Password(password)
	, m_LFSPatterns(lfsPatterns)
	, m_LFSPathSpec(m_GitAPI.CreatePathSpec(lfsPatterns))
{
}

std::vector<char> LFSClient::CreatePointerFileContents(const std::vector<char>& fileContents) const
{
	// From the specs: "an empty file is the pointer for an empty file. That is, empty files are passed through LFS without any change."
	if (fileContents.empty())
	{
		return {};
	}

	std::string hexHash = CalcOID(fileContents);

	std::string pointerContent = "version https://git-lfs.github.com/spec/v1\n"
	                             "oid sha256:"
	    + hexHash + "\n"
	                "size "
	    + std::to_string(fileContents.size()) + "\n";

	return std::vector<char>(pointerContent.begin(), pointerContent.end());
}

std::string LFSClient::CalcOID(const std::vector<char>& fileContents)
{
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char*>(fileContents.data()), fileContents.size(), hash);

	std::string oid;
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		char hex[3];
		snprintf(hex, sizeof(hex), "%02x", hash[i]);
		oid += hex;
	}

	return oid;
}

LFSClient::UploadResult LFSClient::UploadFile(const std::vector<char>& fileContents) const
{
	std::unique_ptr<Communicator> comm;
	switch (m_LFSAPI)
	{
	case API::LFS:
		comm = Communicator::CreateLFS(m_ServerUrl, m_Username, m_Password);
		break;
	case API::S3:
		comm = Communicator::CreateS3(m_ServerUrl, m_Bucket, m_Repository, m_Username, m_Password);
		break;
	default:
		break;
	}

	if (!comm)
	{
		return UploadResult::UnknownAPI;
	}

	return comm->UploadFile(fileContents);
}

bool LFSClient::IsLFSTracked(const std::string& filePath) const
{
	return git_pathspec_matches_path(m_LFSPathSpec.get(), GIT_PATHSPEC_IGNORE_CASE, filePath.c_str()) == 1;
}

std::vector<char> LFSClient::GetGitAttributesContents() const
{
	std::vector<char> result;
	for (const auto& pattern : m_LFSPatterns)
	{
		const std::string line = pattern + " filter=lfs diff=lfs merge=lfs -text\n";
		result.insert(result.end(), line.begin(), line.end());
	}

	return result;
}