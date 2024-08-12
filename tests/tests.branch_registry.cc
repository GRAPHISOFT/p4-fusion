#include "tests.branch_registry.h"

#include <string>

#include "tests.common.h"
#include "branch_registry.h"

#include "rapidjson/rapidjson.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

int TestBranchRegistry ()
{
    TEST_START();

	// Create branch structure

	BranchRegistry r1;

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

	// Test basic operations

	const std::string pUnknownBranchPath = "//Depot/Unkown/Branches/1";

	TEST(r1.Contains(pUnknownBranchPath), false);
	TEST(r1.GetBranchID(pUnknownBranchPath), BranchRegistry::InvalidBranchID);

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

	// Test (de-)serialization

	const char* pReference = R"([{"id":0,"name":"Master","path":"//Development/Main","parentCandidates":[]},{"id":1,"name":"Feature-1","path":"//Project/Branches/Feature-1","parentCandidates":[{"id":0,"count":10},{"id":2,"count":5}]},{"id":2,"name":"Feature-1-DEV","path":"//Project/Branches/Feature-1-DEV","parentCandidates":[{"id":1,"count":1}]},{"id":3,"name":"v1.0.1","path":"//Releases/v1.0.1","parentCandidates":[{"id":0,"count":1}]}])";
	rapidjson::Document refDoc;
	refDoc.Parse(pReference);

	rapidjson::Document actualDoc = r1.SerializeToJSON();
	TEST (actualDoc, refDoc);

	BranchRegistry r2 = BranchRegistry::DeserializeFromJSON(refDoc);

	TEST(r1, r2);
	TEST(r1.SerializeToJSON(), r2.SerializeToJSON());
	TEST(refDoc, r2.SerializeToJSON());

	// Add new branch to object deserialized from JSON representation
	const std::string newProjectPath = "//Project/Branches/NewProject";
	r2.Add({ "NewProject", newProjectPath });
	r2.AddParentRef(r2.GetBranchID(newProjectPath), r2.GetBranchID(pParentPath));

	TEST(r2.Contains(newProjectPath), true);

	BranchRegistry::ParentCandidates newProjectParentCandidates = { { r1.GetBranchID(pParentPath), 1 } };
	TEST(r2.GetBranchEntry(newProjectPath).parentCandidates, newProjectParentCandidates);

	const char* pModifiedReference = R"([{"id":0,"name":"Master","path":"//Development/Main","parentCandidates":[]},{"id":1,"name":"Feature-1","path":"//Project/Branches/Feature-1","parentCandidates":[{"id":0,"count":10},{"id":2,"count":5}]},{"id":2,"name":"Feature-1-DEV","path":"//Project/Branches/Feature-1-DEV","parentCandidates":[{"id":1,"count":1}]},{"id":3,"name":"v1.0.1","path":"//Releases/v1.0.1","parentCandidates":[{"id":0,"count":1}]},{"id":4,"name":"NewProject","path":"//Project/Branches/NewProject","parentCandidates":[{"id":0,"count":1}]}])";

	rapidjson::Document modifiedRefDoc;
	modifiedRefDoc.Parse(pModifiedReference);
	TEST (r2.SerializeToJSON(), modifiedRefDoc);

	TEST_END();

	return TEST_EXIT_CODE();
}