#pragma once

#include <cstdint>
#include <string>

#include "utils/p4_path.h"

#include "rapidjson/document.h"

struct BranchInfo {
    P4Path::Part branchName;
    P4Path branchPath;

    bool operator==(const BranchInfo& other) const
    {
        return branchName == other.branchName && branchPath == other.branchPath;
    }

    bool operator!=(const BranchInfo& other) const
    {
        return !(*this == other);
    }
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

        bool operator==(const Entry& other) const {
            return id == other.id &&
                   info == other.info &&
                   parentCandidates == other.parentCandidates;
        }

        bool operator!=(const Entry& other) const {
            return !(*this == other);
        }
    };;

    static const BranchID InvalidBranchID = std::numeric_limits<BranchID>::max();

    BranchID Add(const BranchInfo& branch);
    void AddParentRef(BranchID childID, P4Path parentPath);
    void AddParentRef(BranchID childID, BranchID parentID);

    bool Contains(const std::string& path) const;
    
    BranchID GetBranchID(const P4Path& path) const;
    const Entry& GetBranchEntry(const P4Path& path) const;

    void RenameBranch(const std::string& path, const std::string& newName);

    rapidjson::Value SerializeToJSON(rapidjson::Document& doc) const;
    [[nodiscard]] static BranchRegistry DeserializeFromJSON(const rapidjson::Value& value);

    bool operator==(const BranchRegistry& other) const;
    bool operator!=(const BranchRegistry& other) const;

private:
    BranchID m_nextID = 0;

    std::unordered_map<BranchID, Entry> m_branchesByID;
    std::unordered_map<P4Path, BranchID> m_branchesByPath;

    bool Contains(BranchID id) const;
};