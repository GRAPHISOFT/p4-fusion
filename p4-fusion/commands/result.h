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

	// ClientUser base class contains a std::mutex which is neither copyable nor movable.
	Result(const Result&) = delete;
	Result& operator=(const Result&) = delete;
	Result(Result&& other) noexcept;
	Result& operator=(Result&& other) noexcept;

	void HandleError(Error* e) override;

	const Error& GetError() const { return m_Error; }
};
