/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "result.h"

Result::Result(Result&& other) noexcept
    : ClientUser()
    , m_Error(other.m_Error)
{
}

Result& Result::operator=(Result&& other) noexcept
{
	if (this != &other)
	{
		m_Error = other.m_Error;
	}
	return *this;
}

void Result::HandleError(Error* e)
{
	StrBuf str;
	e->Fmt(&str);
	ERR("Received error: " << e->FmtSeverity() << " " << str.Text());
	m_Error = *e;
}
