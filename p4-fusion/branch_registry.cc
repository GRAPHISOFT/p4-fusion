#include "branch_registry.h"

void BranchRegistry::Add(const BranchInfo& branch, BranchID parentID)
{
    Entry entry = { m_nextID, branch, parentID };
    m_branchesByID.insert({ m_nextID, entry });
    m_branchesByPath.insert({ branch.branchPath, entry });

    ++m_nextID;
}

bool BranchRegistry::Contains (const std::string& path) const
{
    return GetBranchID(path) != InvalidBranchID;
}

BranchRegistry::BranchID BranchRegistry::GetBranchID (const P4Path& path) const
{
    return m_branchesByPath.find(path) != m_branchesByPath.end() ?
        m_branchesByPath.at(path).id : InvalidBranchID;
}

const BranchRegistry::Entry& BranchRegistry::GetBranchEntry(const P4Path& path) const
{
    return m_branchesByPath.at(path);
}

void BranchRegistry::RenameBranch(const std::string& path, const std::string& newName)
{
    Entry& entry = m_branchesByPath.at(path);
    entry.info.branchName = newName;
}
