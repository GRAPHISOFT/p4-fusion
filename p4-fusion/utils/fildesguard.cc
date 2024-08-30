#include "fildesguard.h"

#include <unistd.h>

FildesGuard::FildesGuard(int fd) : m_fd(fd) {}

FildesGuard::~FildesGuard() {
    close(m_fd);
}