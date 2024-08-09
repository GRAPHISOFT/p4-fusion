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
	auto f1ID = r1.Add({ "Feature-1", pChild1Path });
	for(auto i = 0; i < 10; ++i)
		r1.AddParentRef(f1ID, r1.GetBranchID(pParentPath));

	auto f1DevID = r1.Add({ "Feature-1-DEV", pGrandChildPath });
	r1.AddParentRef(f1DevID, pChild1Path);

	for(auto i = 0; i < 5; ++i)
		r1.AddParentRef(f1ID, r1.GetBranchID(pGrandChildPath));
	
	auto v101ID =r1.Add({ "v1.0.1", pChild2Path });
	r1.AddParentRef(v101ID, pParentPath);

	TEST(r1.Contains(pParentPath), true);
	TEST(r1.Contains(pChild1Path), true);
	TEST(r1.Contains(pGrandChildPath), true);
	TEST(r1.Contains(pChild2Path), true);

	TEST(r1.GetBranchEntry(pParentPath).parentCandidates, BranchRegistry::NoParent);

	BranchRegistry::ParentCandidates child1ParentCandidates = { { r1.GetBranchID(pParentPath), 10 },
																{ r1.GetBranchID(pGrandChildPath), 5 }  };
	TEST(r1.GetBranchEntry(pChild1Path).parentCandidates, child1ParentCandidates);

	BranchRegistry::ParentCandidates child2ParentCandidates = { { r1.GetBranchID(pParentPath), 1 } };
	TEST(r1.GetBranchEntry(pChild2Path).parentCandidates, child2ParentCandidates);

	BranchRegistry::ParentCandidates grandChildParentCandidates = { { r1.GetBranchID(pChild1Path), 1 } };
	TEST(r1.GetBranchEntry(pGrandChildPath).parentCandidates, grandChildParentCandidates);

	TEST (r1.GetBranchEntry(pParentPath).info.branchName, "Main");
	r1.RenameBranch(pParentPath, "Master");
	TEST (r1.GetBranchEntry(pParentPath).info.branchName, "Master");

	TEST_END();

	return TEST_EXIT_CODE();
}