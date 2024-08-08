#include "tests.branch_registry.h"

#include <string>

#include "tests.common.h"
#include "branch_registry.h"

int TestBranchRegistry ()
{
    TEST_START();

	BranchRegistry r1;

	const std::string pUnknownBranchPath = "//Depot/Unkown/Branches/1";

	TEST(r1.Contains(pUnknownBranchPath), false);
	TEST(r1.GetBranchID(pUnknownBranchPath), BranchRegistry::InvalidBranchID);

	const std::string pParentPath = "//Development/Main";
	const std::string pChild1Path = "//Project/Branches/Feature-1";
	const std::string pGrandChildPath = "//Project/Branches/Feature-1-DEV";
	const std::string pChild2Path = "//Releases/v1.0.1";

	r1.Add({ "Main", pParentPath });
	r1.Add({ "Feature-1", pChild1Path }, r1.GetBranchID(pParentPath));
	r1.Add({ "Feature-1-DEV", pGrandChildPath }, r1.GetBranchID(pChild1Path));
	r1.Add({ "v1.0.1", pChild2Path }, r1.GetBranchID(pParentPath));

	TEST(r1.Contains(pParentPath), true);
	TEST(r1.Contains(pChild1Path), true);
	TEST(r1.Contains(pGrandChildPath), true);
	TEST(r1.Contains(pChild2Path), true);

	TEST(r1.GetBranchEntry(pParentPath).parentID, BranchRegistry::InvalidBranchID);
	TEST(r1.GetBranchID(pParentPath), r1.GetBranchEntry(pChild1Path).parentID);
	TEST(r1.GetBranchID(pChild1Path), r1.GetBranchEntry(pGrandChildPath).parentID);
	TEST(r1.GetBranchID(pParentPath), r1.GetBranchEntry(pChild2Path).parentID);

	TEST (r1.GetBranchEntry(pParentPath).info.branchName, "Main");
	r1.RenameBranch(pParentPath, "Master");
	TEST (r1.GetBranchEntry(pParentPath).info.branchName, "Master");

	TEST_END();

	return TEST_EXIT_CODE();
}