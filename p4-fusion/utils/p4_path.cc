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

std::string NormalizeP4Path(const std::string& path)
{
    std::string result = STDHelpers::ToLower(path);
    if(STDHelpers::EndsWith(result, "/"))
    {
        result.pop_back();
    }
    
    return result;
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
    m_normalizedPart = STDHelpers::ToLower(m_rawPart);
}

P4Path::Part::Part(const char* pPartStr)
{
    m_rawPart = pPartStr;
    m_normalizedPart = STDHelpers::ToLower(pPartStr);
}

const std::string& P4Path::Part::AsString() const
{
    return m_rawPart;
}

bool P4Path::Part::operator==(const Part& other) const
{
    return m_normalizedPart == other.m_normalizedPart;
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
    m_normalizedPath = NormalizeP4Path(m_rawPath);
}

P4Path::P4Path(Parts::const_iterator begin, Parts::const_iterator end)
{
    std::string path = "//";
    for (auto it = begin; it != end; ++it)
    {
        path += it->AsString() + "/";
    }

    m_rawPath = path;
    m_normalizedPath = NormalizeP4Path(m_rawPath);
}

P4Path::P4Path(const Parts& parts):
    P4Path(parts.begin(), parts.end())
{
}

P4Path::Parts P4Path::Splice(size_t index_begin, size_t index_end) const
{
    auto parts = GetParts();

    if (index_begin >= parts.size() || index_end >= parts.size())
    {
        throw std::out_of_range("Index out of range");
    }

    if (index_begin >= index_end)
    {
        throw std::invalid_argument("Invalid splice range");
    }

    return { parts.begin() + index_begin, parts.begin() + index_end + 1 };  // +1: half-open range blues
}

const std::string& P4Path::AsString() const
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
	return m_normalizedPath == other.m_normalizedPath;
}

bool P4Path::operator!=(const P4Path& other) const
{
    return !(*this == other);
}