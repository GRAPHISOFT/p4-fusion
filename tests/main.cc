/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include <iostream>

#include "tests.common.h"
#include "tests.utils.h"
#include "tests.git.h"
#include "tests.p4_path.h"
#include "tests.branch_registry.h"

int main()
{
	TEST_REPORT("Utils", TestUtils());
	TEST_REPORT("GitAPI", TestGitAPI());
	TEST_REPORT("P4Path", TestP4Path());
	TEST_REPORT("BranchRegistry", TestBranchRegistry());

	SUCCESS("All test cases passed");
	return 0;
}
