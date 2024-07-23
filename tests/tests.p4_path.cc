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

	{
		bool threw = false;
		try
		{
			P4Path p2("something");
		}
		catch (const P4Path::InvalidPathException& e)
		{
			threw = true;
		}

		TEST(threw, true);

		threw = false;

		try
		{
			P4Path p3("/some/path/to/file.txt");
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