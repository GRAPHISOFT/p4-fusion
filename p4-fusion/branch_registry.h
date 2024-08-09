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
    using ParentCandidates = std::unordered_map<BranchID, uint32_t>;
    static const ParentCandidates NoParent;

    struct Entry {
        BranchID id;
        BranchInfo info;
        ParentCandidates parentCandidates;
    };

    static const BranchID InvalidBranchID = std::numeric_limits<BranchID>::max();

    BranchID Add(const BranchInfo& branch);
    void AddParentRef(BranchID childID, P4Path parentPath);
    void AddParentRef(BranchID childID, BranchID parentID);

    bool Contains(const std::string& path) const;
    
    BranchID GetBranchID(const P4Path& path) const;
    const Entry& GetBranchEntry(const P4Path& path) const;

    void RenameBranch(const std::string& path, const std::string& newName);

private:
    BranchID m_nextID = 0;

    std::unordered_map<BranchID, Entry> m_branchesByID;
    std::unordered_map<P4Path, BranchID> m_branchesByPath;

    bool Contains(BranchID id) const;
};