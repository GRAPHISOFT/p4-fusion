#pragma once

#include <string>
#include <vector>

class P4Path {
public:
    class InvalidPathException : public std::exception {
    public:
        InvalidPathException(const std::string& path);

        const char* what() const noexcept override;

    private:
        std::string m_path;
        mutable std::string m_what;
    };

    class Part {
    public:
        Part(const std::string& part);
        Part(std::string&& part);
        Part(const char* pPartStr);

        const std::string& GetPart() const;

        bool operator==(const Part& other) const;
        bool operator!=(const Part& other) const;

    private:
        std::string m_rawPart;
        std::string m_lowerPart;
    };

    P4Path(const std::string& path);
    P4Path(std::string&& path);

    const std::string& GetPath() const;
    Part GetDepotName() const;

    std::vector<Part> GetParts () const;

    // Comparison operators
    bool operator==(const P4Path& other) const;
    bool operator!=(const P4Path& other) const;

private:
    std::string m_rawPath;
    std::string m_lowerPath;
};

namespace std {
    template <>
    struct hash<P4Path> {
        size_t operator()(const P4Path& path) const {
            return std::hash<std::string>()(path.GetPath());
        }
    };
}