#include "environment.h" // Tùy vào tên file header thực tế của bạn
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

NativeEnvironment::NativeEnvironment(const std::string& dir) {
    // 1. Kéo đường dẫn về ngay tại thư mục Project hiện tại (Nơi bạn chạy mã)
    fs::path project_root = fs::current_path();
    work_dir_ = (project_root / dir).string();
}

void NativeEnvironment::setup() {
    if (!fs::exists(work_dir_)) {
        fs::create_directories(work_dir_);
    }
    std::cout << "[NativeEnv] Workspace ready at project root: "
              << fs::absolute(work_dir_).string() << "\n";
}

void NativeEnvironment::teardown() {
    // 2. ĐÃ XÓA LỆNH 
    // fs::remove_all(work_dir_)
    // Chỉ cập nhật trạng thái và in log thông báo giữ lại file
    if (fs::exists(work_dir_)) {
        std::cout << "[NativeEnv] Teardown complete. : " << work_dir_ << "\n";
    }
}