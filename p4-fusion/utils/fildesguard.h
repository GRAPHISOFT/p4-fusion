#pragma once

class FildesGuard {
public:
    explicit FildesGuard(int fd);

    ~FildesGuard();

    int Get() const { return m_fd; }

private:
    int m_fd;
};