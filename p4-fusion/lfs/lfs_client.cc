/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "lfs_client.h"
#include "openssl/sha.h"
#include "lfs/communication/lfscomm.h"
#include "lfs/communication/s3comm.h"

namespace
{

std::vector<std::string> SelectPatterns(const std::vector<std::string>& lfsPatterns, bool exclude)
{
	std::vector<std::string> result;
	for (const std::string& pattern : lfsPatterns)
	{
		const bool isExclude = !pattern.empty() && pattern[0] == '!';
		if (isExclude == exclude)
		{
			result.push_back(isExclude ? pattern.substr(1) : pattern);
		}
	}
	return result;
}

}

LFSClient::LFSClient(GitAPI& gitAPI, std::unique_ptr<Communicator> communicator, const std::vector<std::string>& lfsPatterns)
    : m_GitAPI(gitAPI)
    , m_Communicator(std::move(communicator))
    , m_LFSPatterns(SelectPatterns(lfsPatterns, false))
    , m_LFSExcludePatterns(SelectPatterns(lfsPatterns, true))
    , m_LFSPathSpec(m_GitAPI.CreatePathSpec(m_LFSPatterns))
    , m_LFSExcludePathSpec(m_GitAPI.CreatePathSpec(m_LFSExcludePatterns))
{
}

std::vector<char> LFSClient::CreatePointerFileContents(const std::vector<char>& fileContents) const
{
	// From the specs: "an empty file is the pointer for an empty file. That is, empty files are passed through LFS without any change."
	if (fileContents.empty())
	{
		return {};
	}

	const std::string hexHash = CalcOID(fileContents);

	const std::string pointerContent = "version https://git-lfs.github.com/spec/v1\n"
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

Communicator::UploadResult LFSClient::UploadFile(const std::vector<char>& fileContents) const
{
	if (!m_Communicator)
	{
		return Communicator::UploadResult::Error;
	}

	return m_Communicator->UploadFile(fileContents);
}

bool LFSClient::IsLFSTracked(const std::string& filePath) const
{
	// An empty pathspec matches every path, so the pattern lists have to be checked first
	if (m_LFSPatterns.empty())
	{
		return false;
	}
	if (!m_LFSExcludePatterns.empty() && git_pathspec_matches_path(m_LFSExcludePathSpec.get(), GIT_PATHSPEC_IGNORE_CASE, filePath.c_str()) == 1)
	{
		return false;
	}
	return git_pathspec_matches_path(m_LFSPathSpec.get(), GIT_PATHSPEC_IGNORE_CASE, filePath.c_str()) == 1;
}

std::vector<std::string> LFSClient::GetGitAttributesLines() const
{
	std::vector<std::string> result;
	for (const auto& pattern : m_LFSPatterns)
	{
		result.push_back(pattern + " filter=lfs diff=lfs merge=lfs -text");
	}
	// Exclusions come last, so they override the LFS lines above. "text=auto" restores the default
	// text detection, without corrupting binary files, as "text" would do.
	for (const auto& pattern : m_LFSExcludePatterns)
	{
		result.push_back(pattern + " !filter !diff !merge text=auto");
	}

	return result;
}