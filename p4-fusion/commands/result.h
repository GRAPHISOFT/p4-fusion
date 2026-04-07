/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"

class Result : public ClientUser
{
	Error m_Error;

public:
	Result() = default;
	~Result() override = default;

	// ClientUser contains a std::mutex which is neither copyable nor movable.
	// Move operations default-construct a fresh ClientUser base and transfer
	// only Result-level data.  The ClientUser state is only needed during
	// ClientApi::Run() and is not required after the command completes.
	Result(Result&& other) noexcept
	    : ClientUser()
	    , m_Error(other.m_Error)
	{
	}
	Result& operator=(Result&& other) noexcept
	{
		if (this != &other)
		{
			m_Error = other.m_Error;
		}
		return *this;
	}

	Result(const Result&) = delete;
	Result& operator=(const Result&) = delete;

	void HandleError(Error* e) override;

	const Error& GetError() const { return m_Error; }
};
