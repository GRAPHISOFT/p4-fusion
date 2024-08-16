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

P4Path::Part::Part(const std::string& part): Part(std::string(part))
{
}

P4Path::Part::Part(std::string&& part)
{
    m_rawPart = std::move(part);
    m_lowerPart = STDHelpers::ToLower(m_rawPart);
}

P4Path::Part::Part(const char* pPartStr)
{
    m_rawPart = pPartStr;
    m_lowerPart = STDHelpers::ToLower(pPartStr);
}

bool P4Path::Part::operator==(const Part& other) const
{
    return m_lowerPart == other.m_lowerPart;
}

bool P4Path::Part::operator!=(const Part& other) const
{
    return !(*this == other);
}

P4Path::P4Path(const std::string& path): P4Path(std::string(path))
{
}

P4Path::P4Path(std::string&& path)
{
    if (!IsValidP4Path (path))
    {
        throw InvalidPathException(path);
    }

    m_rawPath = std::move(path);
    m_lowerPath = STDHelpers::ToLower(m_rawPath);
}

const std::string& P4Path::GetPath() const
{
    return m_rawPath;
}

P4Path::Part P4Path::GetDepotName () const
{
    return GetParts ()[0];
}

std::vector<P4Path::Part> P4Path::GetParts() const
{
    std::vector<P4Path::Part> result;
    std::string remainder = m_rawPath;
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
	return m_lowerPath == other.m_lowerPath;
}

bool P4Path::operator!=(const P4Path& other) const
{
    return !(*this == other);
}