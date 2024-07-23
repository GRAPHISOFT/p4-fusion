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

    explicit P4Path(const std::string& path);

    const std::string& GetPath() const;
    std::string GetDepotName() const;

    std::vector<std::string> GetParts () const;

private:
    std::string m_path;
};