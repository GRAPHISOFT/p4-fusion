/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "communicator.h"
#include "credentials.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class LFSComm : public Communicator
{
public:
	LFSComm(const std::string& serverURL, const Credentials& creds);

	virtual ~LFSComm() = default;

	UploadResult UploadFile(const std::vector<char>& fileContents) const override;

private:
	// Per-OID in-flight tracking. When two threads try to upload identical
	// content (same OID) concurrently, the second one waits for the first to
	// finish and then reports AlreadyExists, avoiding a same-OID PUT race
	// against the LFS server.
	struct InFlightUpload
	{
		std::mutex mutex;
		std::condition_variable cv;
		bool done = false;
		UploadResult result = UploadResult::Error;
	};

	const std::string m_ServerURL;
	const Credentials m_Creds;

	mutable std::mutex m_InFlightMutex;
	mutable std::unordered_map<std::string, std::shared_ptr<InFlightUpload>> m_InFlightUploads;
};
