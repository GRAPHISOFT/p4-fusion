#pragma once

#include <cstdint>
#include <string>

#include "utils/p4_path.h"

struct BranchInfo {
    std::string branchName;
    P4Path branchPath;
};

// This class is a registry of branches, storing branches' name, path, and parent branch (if there is one).
class BranchRegistry {
public:
    using BranchID = uint16_t;

    struct Entry {
        BranchID id;
        BranchInfo info;
        BranchID parentID;
    };

    static const BranchID InvalidBranchID = std::numeric_limits<BranchID>::max();

    void Add (const BranchInfo& branch, BranchID parentID = InvalidBranchID);
    bool Contains (const std::string& path) const;
    
    BranchID GetBranchID (const P4Path& path) const;
    const Entry& GetBranchEntry (const P4Path& path) const;

    void RenameBranch(const std::string& path, const std::string& newName);

private:
    BranchID m_nextID = 0;

    std::unordered_map<BranchID, Entry> m_branchesByID;
    std::unordered_map<P4Path, Entry> m_branchesByPath;
};