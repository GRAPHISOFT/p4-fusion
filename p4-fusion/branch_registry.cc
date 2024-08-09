#include "branch_registry.h"

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
    if(!Contains(parentPath.GetPath()))
    {
        throw std::runtime_error("Branch cannot be added as parent, because it's not in the registry!");
    }

    AddParentRef(childID, GetBranchID(parentPath));
}

void BranchRegistry::AddParentRef(BranchID childID, BranchID parentID)
{
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

bool BranchRegistry::Contains (const std::string& path) const
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

bool BranchRegistry::Contains(BranchID id) const
{
    return m_branchesByID.find(id) != m_branchesByID.end();
}