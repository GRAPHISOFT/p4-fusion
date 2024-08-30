#include "auto_branching.h"

#include <fcntl.h>
#include <unistd.h>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "utils/fildesguard.h"

namespace {

std::string RepoPathToRegistryFilePath(const std::string& repoPath)
{
    const std::string parentPath = repoPath.substr(0, repoPath.find_last_of("/"));
    const std::string repoFolderName = parentPath.substr(parentPath.find_last_of("/") + 1);
    const std::string registryFileName = "branch_registry_" + repoFolderName + ".json";
    return parentPath + "/" + registryFileName;
}

// From a "parent" and "child" file path, this function determines the paths and names of the branches that the files belong to
std::pair<BranchInfo, BranchInfo> DetermineBranchInfo(const P4Path& parentPath, const P4Path& childPath)
{
    // The algorithm is dead simple: iterate on the path components of the parent and child paths starting from the end,
    //  and when there is a difference, that's where the branch is located. The name of the branch is the last component.
    // E.g., if we have //Products/A/Main/test.cpp and //Products/A/Project1/test.cpp, then the parent branch is
    //  @ //Products/A/Main with branch name Main, and the child branch is @ //Products/A/Project1 with branch name Project1

    // There are some edge cases to consider as well, e.g.: //Products/Main/test.cpp and //Releases/v1.1/Main/test.cpp
    //  In this case, the above approach would yield parent branch //Products and //Releases/v1.1
    // Overall, it's practically impossible to account for all edge cases, since Perforce provides an amazing (or
    //  horrifying, if you will) degree of freedom

    const std::vector<P4Path::Part>& parentPathParts = parentPath.GetParts();
    const std::vector<P4Path::Part>& childPathParts = childPath.GetParts();

    const size_t nParentParts = parentPathParts.size();
    const size_t nChildParts = childPathParts.size();

    auto parentPartIter = parentPathParts.rbegin();
    auto childPartIter = childPathParts.rbegin();

    size_t nParentPartsRemaining = nParentParts;
    size_t nChildPartsRemaining = nChildParts;

    while(true) {
        if(*parentPartIter != *childPartIter) {
            // We might have found the two branches' locations and names

            // Check for edge case: a branch should never be located at a Depot root
            // If that would be the case, go with the results candidates of the previous iteration
            if(nParentPartsRemaining <= 2 || nChildPartsRemaining <= 2) {
                // TODO
            } else {
                /*const P4Path parentBranchPath(parentPath.GetPath().substr(0, parentPath.GetPath().size() - parentPartIter->GetPart().size()));
                const P4Path childBranchPath(childPath.GetPath().substr(0, childPath.GetPath().size() - childPartIter->GetPart().size()));

                const BranchInfo parentBranchInfo = { parentPartIter->GetPart(), parentBranchPath };
                const BranchInfo childBranchInfo = { childPartIter->GetPart(), childBranchPath };

                return { parentBranchInfo, childBranchInfo };*/
            }
        }

        if(nParentPartsRemaining == 0 && nChildPartsRemaining == 0) {
            throw std::runtime_error("Parent and child paths are identical");
        }

        ++parentPartIter;
        ++childPartIter;
        --nParentPartsRemaining;
        --nChildPartsRemaining;
    }
}

}

bool BranchRegistryFileExistsForRepo(const std::string& repoPath)
{
    const std::string registryFilePath = RepoPathToRegistryFilePath(repoPath);

    return access(registryFilePath.c_str(), F_OK) != -1;
}

std::tuple<uint32_t, BranchRegistry> LoadLastProcessedCLAndBranchRegistryFromFile(const std::string& repoPath)
{
    const std::string registryFilePath = RepoPathToRegistryFilePath(repoPath);

    const FildesGuard fd(open(registryFilePath.c_str(), O_RDONLY));

    if (fd.Get() == -1) {
        throw std::runtime_error("Failed to open branch registry file");
    }

    off_t fileSize = lseek(fd.Get(), 0, SEEK_END);
    if (fileSize == -1) {
        throw std::runtime_error("Failed to get file size of branch registry file");
    }

    std::unique_ptr<char[]> buffer(new char[fileSize]);

    ssize_t bytesRead = pread(fd.Get(), buffer.get(), fileSize, 0);
    if (bytesRead == -1) {
        throw std::runtime_error("Unable to read branch registry file");
    }

    rapidjson::Document document;
    document.Parse(buffer.get(), bytesRead);

    // Read the last change list number from the registry file: an int with key named "lastProcessedCL"
    uint32_t lastProcessedCL = document["lastProcessedCL"].GetInt();

    // Get the object that represents the branch registry: an array with key named "branchRegistry"
    const rapidjson::Value& branchRegistryJSON = document["branchRegistry"];

    BranchRegistry branchRegistry = branchRegistry.DeserializeFromJSON(branchRegistryJSON);

    return { lastProcessedCL, branchRegistry };
}

void SaveLastProcessedCLAndBranchRegistryToFile(const std::string& repoPath, uint32_t lastProcessedCL, const BranchRegistry& branchRegistry)
{
    
}

void ProcessCLForBranchRegistry(P4API& p4, uint32_t clNum, BranchRegistry& branchRegistry, BranchRegistry::BranchID& rootID)
{
    FileLogResult fileLogResult = p4.FileLog(std::to_string(clNum));

    for(const auto& fileData : fileLogResult.GetFileData()) {
        if(fileData.GetHow() != "branch from") {
            continue;   // For discovering branches, this is the only type that bears any useful information
        }

        const P4Path childFilePath = fileData.GetDepotFile();
        const P4Path parentFilePath = fileData.GetFromDepotFile();

        // Weird, but it is (or was?) possible to branch a file from itself (from a previous revision, which was later deleted)
        if(childFilePath == parentFilePath){
            continue;
        }

        auto branchInfos = DetermineBranchInfo(parentFilePath, childFilePath);
        BranchInfo& parentBranchInfo = branchInfos.first;
        BranchInfo& childBranchInfo = branchInfos.second;
    }
}