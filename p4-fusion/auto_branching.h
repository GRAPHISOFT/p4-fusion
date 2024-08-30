#pragma once

#include "branch_registry.h"
#include "commands/change_list.h"
#include "p4_api.h"

bool BranchRegistryFileExistsForRepo (const std::string& repoPath);
std::tuple<uint32_t, BranchRegistry> LoadLastProcessedCLAndBranchRegistryFromFile (const std::string& repoPath);
void SaveLastProcessedCLAndBranchRegistryToFile (const std::string& repoPath, uint32_t lastProcessedCL, const BranchRegistry& branchRegistry);

void ProcessCLForBranchRegistry (P4API& p4, uint32_t clNum, BranchRegistry& branchRegistry, BranchRegistry::BranchID& rootID);