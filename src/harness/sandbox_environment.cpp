#include "environment.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>   // mkdtemp
#include <cstring>   // strerror
#include <cerrno>    // errno

namespace fs = std::filesystem;

SandboxEnvironment::SandboxEnvironment(const std::string& prefix)
    : prefix_(prefix) {}

void SandboxEnvironment::setup() {
    // mkdtemp yêu cầu template kết thúc bằng "XXXXXX"
    std::string tmpl = (fs::temp_directory_path() / (prefix_ + "XXXXXX")).string();

    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');

    if (!mkdtemp(buf.data())) {
        throw std::runtime_error(
            std::string("[SandboxEnv] mkdtemp failed: ") + std::strerror(errno));
    }

    sandbox_dir_ = buf.data();
    active_      = true;
    std::cout << "[SandboxEnv] Isolated sandbox created: " << sandbox_dir_ << "\n";
}

void SandboxEnvironment::teardown() {
    if (active_ && fs::exists(sandbox_dir_)) {
        fs::remove_all(sandbox_dir_);
        active_ = false;
        std::cout << "[SandboxEnv] Sandbox cleaned up: " << sandbox_dir_ << "\n";
    }
}
