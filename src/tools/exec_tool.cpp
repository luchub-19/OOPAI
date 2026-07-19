#include "exec_tool.h"

#include <array>
#include <cstdio>
#include <memory>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
    #define POPEN _popen
    #define PCLOSE _pclose
#else
    #define POPEN popen
    #define PCLOSE pclose
#endif

#include <nlohmann/json.hpp>
using json = nlohmann::json;

// Cập nhật Constructor
ExecTool::ExecTool(std::function<std::string()> pathProvider, int timeoutSeconds, bool allowShellMeta)
    : path_provider_(std::move(pathProvider)), timeout_seconds_(timeoutSeconds), allow_shell_meta_(allowShellMeta) {}

std::string ExecTool::getName() const { return "exec"; }

std::string ExecTool::getDescription() const {
    std::string os_info = 
    #ifdef _WIN32
            "LƯU Ý: Hệ thống đang chạy Windows (cmd.exe). Hãy dùng cú pháp của Windows. ";
    #else
            "LƯU Ý: Hệ thống đang chạy Linux (bash). Hãy dùng cú pháp của Linux. ";
    #endif

        return "Công cụ thực thi lệnh shell trên hệ điều hành và trả về kết quả "
            "(stdout/stderr gộp chung). Chỉ dùng cho lệnh non-interactive, "
            "không dùng cho trình soạn thảo tương tác (nano, vim...). "
            "Lệnh sẽ bị hủy sau " +
            std::to_string(timeout_seconds_) +
            " giây nếu chạy quá lâu. " +
            os_info + 
            "Tham số đầu vào (args) phải là JSON hợp lệ: "
            "{\"command\": \"lệnh shell cần chạy\"}";
}

bool ExecTool::isCommandSafe(const std::string& cmd) const {
    if (allow_shell_meta_) return true;

    static const std::vector<std::string> dangerous = {
        "rm -rf /",  "rm -rf /*", ":(){ :|:& };:", "mkfs",
        "dd if=",    "> /dev/sda", "shutdown",      "reboot",
        " nano ",    " vim ",      " vi ",           "/bin/nano",
        "/usr/bin/vim", "nano ", "vim ",
    };
    for (const auto& pattern : dangerous) {
        if (cmd.find(pattern) != std::string::npos) return false;
    }
    return true;
}

std::optional<std::string> ExecTool::runCommand(const std::string& cmd,
                                                int timeoutSeconds) {
    std::ostringstream wrapped;
    
    std::string current_workspace = path_provider_ ? path_provider_() : "";

    // 1. Chèn lệnh di chuyển thư mục (cd) trước khi chạy lệnh chính
    if (!current_workspace.empty()) {
#ifdef _WIN32
        // Trên Windows, dùng cd /d để có thể chuyển đổi ổ đĩa (vd: từ C: sang D:)
        wrapped << "cd /d \"" << current_workspace << "\" && ";
#else
        wrapped << "cd \"" << current_workspace << "\" && ";
#endif
    }
    
    // 2. Thêm lệnh gốc và cấu hình timeout
#ifdef _WIN32
    wrapped << cmd << " 2>&1";
#else
    wrapped << "timeout " << timeoutSeconds << "s " << cmd << " 2>&1";
#endif

    std::array<char, 4096> buffer{};
    std::string result;

    std::unique_ptr<FILE, int(*)(FILE*)> pipe(
        POPEN(wrapped.str().c_str(), "r"), PCLOSE);

    if (!pipe) {
        return std::nullopt; 
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

std::optional<std::string> ExecTool::execute(const std::string& args) {
    json j;
    try {
        j = json::parse(args);
    } catch (const json::parse_error& e) {
        return std::string("Loi parse JSON dau vao: ") + e.what();
    }

    if (!j.contains("command") || !j["command"].is_string()) {
        return std::string(
            "Loi: JSON args thieu truong 'command' kieu string.");
    }

    std::string command;
    try {
        command = j["command"].get<std::string>();
    } catch (const json::type_error& e) {
        return std::string("Loi kieu du lieu JSON: ") + e.what();
    }

    if (command.empty()) {
        return std::string("Loi: 'command' rong.");
    }

    if (!isCommandSafe(command)) {
        return std::string("Loi: lenh bi chan boi ExecTool safety policy.");
    }

    try {
        auto output = runCommand(command, timeout_seconds_);
        if (!output.has_value()) {
            return std::string("Loi: khong the khoi chay process (popen).");
        }
        if (output->empty()) {
            return std::string(
                "Lenh da thuc thi thanh cong (khong co output tra ve).");
        }
        return output;
    } catch (const std::exception& e) {
        return std::string("Loi thuc thi lenh: ") + e.what();
    }
}