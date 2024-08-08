#include "p4_path.h"

#include "std_helpers.h"

namespace {
    bool IsValidP4Path(const std::string& path)
    {
        // Best-effort validation...

        if(path.empty())
        {
            return false;
        }

        if(!STDHelpers::StartsWith(path, "//"))
        {
            return false;
        }

        if(STDHelpers::Contains(path, "\\"))
        {
            return false;
        }

        return true;
    }
    
}

P4Path::InvalidPathException::InvalidPathException(const std::string& path)
    : m_path(path)
{
}

const char* P4Path::InvalidPathException::what() const noexcept
{
    m_what = std::string ("Invalid P4 path: " + m_path);
    
    return m_what.c_str();
}

P4Path::P4Path(const std::string& path)
{
    if (!IsValidP4Path (path))
    {
        throw InvalidPathException(path);
    }

    m_path = path;
}

const std::string& P4Path::GetPath() const
{
    return m_path;
}

std::string P4Path::GetDepotName () const
{
    return GetParts ()[0];
}

std::vector<std::string> P4Path::GetParts() const
{
    std::vector<std::string> result;
    std::string remainder = m_path;
    STDHelpers::Erase(remainder, "//");

    do {
        std::array<std::string, 2> splitResult = STDHelpers::SplitAt(remainder, '/');
        result.push_back(splitResult[0]);

        remainder = splitResult[1];
    } while (!remainder.empty());

	return result;
}

bool P4Path::operator==(const P4Path& other) const
{
	return m_path == other.m_path;
}

bool P4Path::operator!=(const P4Path& other) const
{
    return !(*this == other);
}