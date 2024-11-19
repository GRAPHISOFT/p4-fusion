#include "branch_registry.h"

#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"

const BranchRegistry::ParentCandidates BranchRegistry::NoParent;

BranchRegistry::BranchID BranchRegistry::Add(const BranchInfo& branch)
{
    Entry entry = { m_nextID, branch, {} };
    m_branchesByID.insert({ m_nextID, entry });
    m_branchesByPath.insert({ branch.branchPath, m_nextID });

    return m_nextID++;
}

void BranchRegistry::AddParentRef(BranchID childID, P4Path parentPath)
{
    if(!Contains(parentPath.AsString()))
    {
        throw std::runtime_error("Branch cannot be added as parent, because it's not in the registry!");
    }

    AddParentRef(childID, GetBranchID(parentPath));
}

void BranchRegistry::AddParentRef(BranchID childID, BranchID parentID)
{
    if(childID == parentID)
    {
        throw std::runtime_error("Branch cannot be added as parent to itself!");
    }

    if(!Contains(parentID))
    {
        throw std::runtime_error("Branch cannot be added as parent, because it's not in the registry!");
    }

    // Add parent ref, if not in our bookkeeping. Increment counter, if it is
    Entry& childEntry = m_branchesByID.at(childID);
    if(childEntry.parentCandidates.find(parentID) == childEntry.parentCandidates.end())
    {
        childEntry.parentCandidates.insert({ parentID, 1 });
    }
    else
    {
        childEntry.parentCandidates.at(parentID)++;
    }
}

bool BranchRegistry::Contains (const P4Path& path) const
{
    return GetBranchID(path) != InvalidBranchID;
}

BranchRegistry::BranchID BranchRegistry::GetBranchID (const P4Path& path) const
{
    return m_branchesByPath.find(path) != m_branchesByPath.end() ?
        m_branchesByPath.at(path) : InvalidBranchID;
}

const BranchRegistry::Entry& BranchRegistry::GetBranchEntry(const P4Path& path) const
{
    return m_branchesByID.at(m_branchesByPath.at(path));
}

void BranchRegistry::RenameBranch(const std::string& path, const std::string& newName)
{
    const BranchID id = m_branchesByPath.at(path);
    m_branchesByID.at(id).info.branchName = newName;
}

rapidjson::Value BranchRegistry::SerializeToJSON(rapidjson::Document& doc) const
{
    rapidjson::Value jsonValue(rapidjson::kArrayType);

    // Create a vector of branch entries sorted by id
    std::vector<const Entry*> sortedEntries;
    sortedEntries.reserve(m_branchesByID.size());
    for (const auto& entry : m_branchesByID)
    {
        sortedEntries.push_back(&entry.second);
    }
    std::sort(sortedEntries.begin(), sortedEntries.end(), [](const Entry* a, const Entry* b) {
        return a->id < b->id;
    });

    for (const auto& branchEntry : sortedEntries)
    {
        rapidjson::Value branchObj(rapidjson::kObjectType);
        branchObj.AddMember("id", branchEntry->id, doc.GetAllocator());
        branchObj.AddMember("name", rapidjson::StringRef(branchEntry->info.branchName.AsString().c_str()), doc.GetAllocator());
        branchObj.AddMember("path", rapidjson::StringRef(branchEntry->info.branchPath.AsString().c_str()), doc.GetAllocator());

        rapidjson::Value parentCandidatesArray(rapidjson::kArrayType);
        std::vector<std::pair<BranchID, uint32_t>> sortedParentCandidates(branchEntry->parentCandidates.begin(), branchEntry->parentCandidates.end());
        std::sort(sortedParentCandidates.begin(), sortedParentCandidates.end(),
            [](std::pair<BranchID, uint32_t> a, std::pair<BranchID, uint32_t> b) {
                return a.first < b.first;
            });

        for (const auto& parent : sortedParentCandidates)
        {
            rapidjson::Value parentObj(rapidjson::kObjectType);
            parentObj.AddMember("id", parent.first, doc.GetAllocator());
            parentObj.AddMember("count", parent.second, doc.GetAllocator());
            parentCandidatesArray.PushBack(parentObj, doc.GetAllocator());
        }

        branchObj.AddMember("parentCandidates", parentCandidatesArray, doc.GetAllocator());

        jsonValue.PushBack(branchObj, doc.GetAllocator());
    }

    return jsonValue;
}

BranchRegistry BranchRegistry::DeserializeFromJSON(const rapidjson::Value& value)
{
    if(!value.IsArray())
    {
        throw std::runtime_error("Branch registry JSON is not an array!");
    }

    BranchRegistry result;

    for (const auto& branch : value.GetArray())
    {
        if(!branch.IsObject())
        {
            throw std::runtime_error("Branch registry JSON array contains non-object element!");
        }
        
        BranchInfo branchInfo = {branch["name"].GetString(), P4Path(branch["path"].GetString())};

        ParentCandidates parentCandidates;
        for (const auto& parent : branch["parentCandidates"].GetArray())
        {
            parentCandidates.insert({  parent["id"].GetUint(), parent["count"].GetUint() });
        }

        Entry entry = {static_cast<BranchID>(branch["id"].GetUint()), branchInfo, parentCandidates};

        result.m_branchesByID.insert({ entry.id, entry });
        result.m_branchesByPath.insert({ branchInfo.branchPath, entry.id });

        ++result.m_nextID;
    }

    return result;
}

bool BranchRegistry::operator==(const BranchRegistry& other) const
{
    return m_branchesByID == other.m_branchesByID && m_branchesByPath == other.m_branchesByPath;
}
 
bool BranchRegistry::operator!=(const BranchRegistry& other) const
{
    return !(*this == other);
}

bool BranchRegistry::Contains(BranchID id) const
{
    return m_branchesByID.find(id) != m_branchesByID.end();
}