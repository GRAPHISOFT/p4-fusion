/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "tests.common.h"
#include "git_api.h"

int TestGitAPI()
{
	TEST_START();

	GitAPI git(false);

	TEST(git.InitializeRepository("/tmp/test-repo"), true);
	git.CreateIndex(nullptr);
	git.AddFileToIndex("foo.txt", { 'x', 'y', 'z' }, false);
	git.Commit(
	    "//a/b/c/...",
	    "12345678",
	    "test.user",
	    "test@user",
	    0,
	    "Test description",
	    10000000,
	    "");
	TEST(git.IsHEADExists(), true);

	auto depoPath = git.GetDepotPathFromLastCommit();
	TEST(depoPath, "//a/b/c/...");
	TEST_NEQ(depoPath, "//a/b/c/d/...");
	TEST_NEQ(depoPath, "//x/y/z/...");
	TEST(git.DetectLatestCL(), "12345678");

	git.RemoveFileFromIndex("foo.txt");
	git.Commit(
	    "//a/b/c/...",
	    "12345679",
	    "test.user.2",
	    "test2@user",
	    0,
	    "Test description",
	    20000000,
	    "");
	TEST(git.IsHEADExists(), true);

	depoPath = git.GetDepotPathFromLastCommit();
	TEST(depoPath, "//a/b/c/...");
	TEST_NEQ(depoPath, "//a/b/c/d/...");
	TEST_NEQ(depoPath, "//x/y/z/...");
	TEST(git.DetectLatestCL(), "12345679");

	git.CloseIndex();

	TEST_END();
	return TEST_EXIT_CODE();
}

int TestGitDirectoryCase()
{
	TEST_START();

	GitAPI git(false);

	// Nothing is committed, so every run starts from the same empty HEAD tree.
	TEST(git.InitializeRepository("/tmp/test-repo-case"), true);
	git.CreateIndex(nullptr);

	// A new directory keeps the incoming case.
	TEST(git.CanonicalizeDirectoryCase("src/a.txt"), "src/a.txt");
	TEST(git.CanonicalizeDirectoryCase("file.txt"), "file.txt");

	git.AddFileToIndex("Src/Thing.txt", { 'a' }, false);
	TEST(git.CanonicalizeDirectoryCase("Src/Other.txt"), "Src/Other.txt");
	TEST(git.CanonicalizeDirectoryCase("src/Other.txt"), "Src/Other.txt");
	TEST(git.CanonicalizeDirectoryCase("SRC/Sub/x.txt"), "Src/Sub/x.txt");
	// Only directories are canonicalized, not the file name.
	TEST(git.CanonicalizeDirectoryCase("src/thing.txt"), "Src/thing.txt");

	// Added with a different directory case: lands in the existing directory.
	git.AddFileToIndex("src/Whatev.txt", { 'b' }, false);
	git.RemoveFileFromIndex("Src/Thing.txt");
	TEST(git.CanonicalizeDirectoryCase("src/x.txt"), "Src/x.txt");

	// Removed with a different directory case: removes the file from the existing directory.
	git.RemoveFileFromIndex("src/Whatev.txt");
	TEST(git.CanonicalizeDirectoryCase("src/x.txt"), "src/x.txt");

	// Emptied directory: the next case wins.
	git.AddFileToIndex("src/New.txt", { 'c' }, false);
	TEST(git.CanonicalizeDirectoryCase("SRC/x.txt"), "src/x.txt");

	// Nested directories, canonicalized level by level.
	git.AddFileToIndex("A/B/c.txt", { 'd' }, false);
	TEST(git.CanonicalizeDirectoryCase("a/b/d.txt"), "A/B/d.txt");
	TEST(git.CanonicalizeDirectoryCase("a/C/d.txt"), "A/C/d.txt");
	TEST(git.CanonicalizeDirectoryCase("a/c/D/e.txt"), "A/c/D/e.txt");

	// Siblings that sort around the searched directory must not confuse the search.
	git.AddFileToIndex("Lib.txt", { 'e' }, false);
	git.AddFileToIndex("Lib-x/a.txt", { 'f' }, false);
	git.AddFileToIndex("Lib.d/a.txt", { 'g' }, false);
	git.AddFileToIndex("Lib0/a.txt", { 'h' }, false);
	git.AddFileToIndex("Libz/a.txt", { 'i' }, false);
	TEST(git.CanonicalizeDirectoryCase("lib/a.txt"), "lib/a.txt");
	TEST(git.CanonicalizeDirectoryCase("lib0/a.txt"), "Lib0/a.txt");
	TEST(git.CanonicalizeDirectoryCase("LIBZ/a.txt"), "Libz/a.txt");
	TEST(git.CanonicalizeDirectoryCase("lib-X/a.txt"), "Lib-x/a.txt");
	TEST(git.CanonicalizeDirectoryCase("lib.txt/a.txt"), "lib.txt/a.txt");

	// Mixed-case siblings sort differently case-insensitively than in byte order ('N' < 'm' < 'n').
	// Only 'N/' exists, the search must still find it after skipping 'm/'.
	git.AddFileToIndex("m/x.txt", { 'j' }, false);
	git.AddFileToIndex("N/y.txt", { 'k' }, false);
	TEST(git.CanonicalizeDirectoryCase("n/z.txt"), "N/z.txt");
	TEST(git.CanonicalizeDirectoryCase("M/z.txt"), "m/z.txt");

	git.CloseIndex();

	TEST_END();
	return TEST_EXIT_CODE();
}
