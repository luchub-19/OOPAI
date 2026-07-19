#include "environment.h" // Tùy vào tên file header thực tế của bạn
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
    // 1. Kéo về thư mục project hiện tại, gom vào thư mục con "workspaces" cho gọn
    fs::path base_dir = fs::current_path() / "workspaces";
    if (!fs::exists(base_dir)) {
        fs::create_directories(base_dir);
    }

    // 2. Tạo template với XXXXXX bên trong project
    std::string tmpl = (base_dir / (prefix_ + "XXXXXX")).string();

    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');

    if (!mkdtemp(buf.data())) {
        throw std::runtime_error(
            std::string("[SandboxEnv] mkdtemp failed: ") + std::strerror(errno));
    }

    sandbox_dir_ = buf.data();
    active_      = true;
    std::cout << "[SandboxEnv] Isolated sandbox created in project: " << sandbox_dir_ << "\n";
}

void SandboxEnvironment::teardown() {
    // Sandbox giữ nguyên đặc tính: Dọn dẹp sạch sẽ sau khi dùng xong
    if (active_ && fs::exists(sandbox_dir_)) {
        fs::remove_all(sandbox_dir_);
        active_ = false;
        std::cout << "[SandboxEnv] Sandbox cleaned up: " << sandbox_dir_ << "\n";
    }
}