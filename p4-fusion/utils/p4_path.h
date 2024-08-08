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

    P4Path(const std::string& path);

    const std::string& GetPath() const;
    std::string GetDepotName() const;

    std::vector<std::string> GetParts () const;

    // Comparison operators
    bool operator==(const P4Path& other) const;
    bool operator!=(const P4Path& other) const;

private:
    std::string m_path;
};

namespace std {
    template <>
    struct hash<P4Path> {
        size_t operator()(const P4Path& path) const {
            return std::hash<std::string>()(path.GetPath());
        }
    };
}