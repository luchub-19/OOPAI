#include "environment.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

NativeEnvironment::NativeEnvironment(const std::string& dir)
    : work_dir_(dir) {}

void NativeEnvironment::setup() {
    fs::create_directories(work_dir_);
    std::cout << "[NativeEnv] Workspace ready: "
              << fs::absolute(work_dir_).string() << "\n";
}

void NativeEnvironment::teardown() {
    if (fs::exists(work_dir_)) {
        fs::remove_all(work_dir_);
        std::cout << "[NativeEnv] Workspace removed: " << work_dir_ << "\n";
    }
}
