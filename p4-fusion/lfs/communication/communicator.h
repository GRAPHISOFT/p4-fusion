/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "lfs/lfs_client.h"

class Communicator
{
protected:
	Communicator() = default;

public:
	static std::unique_ptr<Communicator> CreateLFS(const std::string& serverURL, const std::string& username, const std::string& password);
	static std::unique_ptr<Communicator> CreateS3(const std::string& serverURL, const std::string& bucket, const std::string& repository, const std::string& username, const std::string& password);
	
	virtual ~Communicator() = default;

	virtual LFSClient::UploadResult UploadFile(const std::vector<char>& fileContents) const = 0;
};
