#include "tests.p4_path.h"

#include "tests.common.h"
#include "utils/p4_path.h"

int TestP4Path ()
{
    TEST_START();

	const char* pPathStr = "//Depot/Path/To/file";
	P4Path p(pPathStr);
	TEST(p.GetPath(), pPathStr);
	TEST(p.GetDepotName(), "Depot");
	TEST(p.GetParts(), std::vector<std::string> ({ "Depot", "Path", "To", "file" }));

	const char* pPathWithSlashStr = "//Depot/Branch/2/";
	P4Path p2(pPathWithSlashStr);
	TEST(p2.GetPath(), pPathWithSlashStr);
	TEST(p2.GetDepotName(), "Depot");
	TEST(p2.GetParts(), std::vector<std::string> ({ "Depot", "Branch", "2" }));

	{
		bool threw = false;
		try
		{
			P4Path p("something");
		}
		catch (const P4Path::InvalidPathException& e)
		{
			threw = true;
		}

		TEST(threw, true);

		threw = false;

		try
		{
			P4Path p("/some/path/to/file.txt");
		}
		catch (const P4Path::InvalidPathException& e)
		{
			threw = true;
		}

		TEST(threw, true);

		threw = false;

		try
		{
			P4Path p("");
		}
		catch (const P4Path::InvalidPathException& e)
		{
			threw = true;
		}

		TEST(threw, true);
	}

	TEST_END();
	return TEST_EXIT_CODE();
}