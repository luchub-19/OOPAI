#include "file_tool.h"

#include <fstream>
#include <sstream>
#include <system_error>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace fs = std::filesystem;

FileTool::FileTool(std::function<std::string()> pathProvider,
                    std::size_t maxFileSizeBytes)
    : path_provider_(std::move(pathProvider)),
      max_file_size_bytes_(maxFileSizeBytes) {}

std::string FileTool::getName() const { return "file"; }

std::string FileTool::getDescription() const {
    return "Cong cu tao va thao tac file/folder ben trong workspace cua agent "
           "(khong the truy cap ra ngoai workspace). "
           "Tham so dau vao (args) phai la JSON hop le voi truong 'action' la "
           "mot trong: read, write, append, mkdir, list, delete. "
           "Vi du: "
           "{\"action\": \"read\", \"path\": \"notes/todo.txt\"}, "
           "{\"action\": \"write\", \"path\": \"a.txt\", \"content\": \"hello\"}, "
           "{\"action\": \"append\", \"path\": \"a.txt\", \"content\": \"more\"}, "
           "{\"action\": \"mkdir\", \"path\": \"data\"}, "
           "{\"action\": \"list\", \"path\": \".\"}, "
           "{\"action\": \"delete\", \"path\": \"a.txt\"}. "
           "'path' la duong dan TUONG DOI so voi workspace (khong duoc dung "
           "duong dan tuyet doi hoac '..' de thoat ra ngoai). Kich thuoc file "
           "toi da " + std::to_string(max_file_size_bytes_ / (1024 * 1024)) +
           "MB.";
}

std::optional<fs::path> FileTool::resolveSafePath(
    const fs::path& root, const std::string& relative_path) const {
    // 1. Chan tuyet doi tu dau: fs::path operator/ voi mot path TUYET DOI o ve
    //    phai se THAY THE toan bo path ben trai tren POSIX (vd: root / "/etc/passwd"
    //    == "/etc/passwd") -> phai chan truoc khi ghep, khong thi mat tac dung
    //    sandbox ngay lap tuc.
    fs::path rel(relative_path);
    if (rel.is_absolute()) {
        return std::nullopt;
    }

    // 2. Ghep vao root, chuan hoa. weakly_canonical KHONG doi hoi file/thu muc
    //    phai ton tai san (khac voi canonical), phu hop cho action "write"/
    //    "mkdir" khi target chua duoc tao.
    std::error_code ec;
    fs::path root_canonical = fs::weakly_canonical(root, ec);
    if (ec) {
        return std::nullopt;
    }

    fs::path candidate = fs::weakly_canonical(root / rel, ec);
    if (ec) {
        return std::nullopt;
    }

    // 3. Kiem tra candidate van nam trong root (chan "..", symlink thoat ra
    //    ngoai...). So sanh tung thanh phan path thay vi so sanh chuoi tho de
    //    tranh false-positive kieu "/root2" bat dau bang "/root".
    auto root_it = root_canonical.begin();
    auto cand_it = candidate.begin();
    for (; root_it != root_canonical.end(); ++root_it, ++cand_it) {
        if (cand_it == candidate.end() || *cand_it != *root_it) {
            return std::nullopt;  // candidate ngan hon root hoac le khoi root
        }
    }

    return candidate;
}

std::optional<std::string> FileTool::doRead(const fs::path& p) const {
    std::error_code ec;
    if (!fs::exists(p, ec) || ec) {
        return std::string("Loi: file khong ton tai -> ") + p.string();
    }
    if (fs::is_directory(p, ec)) {
        return std::string("Loi: '") + p.string() +
               "' la thu muc, khong the 'read' (dung 'list' thay the).";
    }

    auto size = fs::file_size(p, ec);
    if (!ec && size > max_file_size_bytes_) {
        return std::string("Loi: file qua lon (") + std::to_string(size) +
               " bytes), vuot gioi han " +
               std::to_string(max_file_size_bytes_) + " bytes.";
    }

    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) {
        return std::string("Loi: khong mo duoc file de doc -> ") + p.string();
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

std::optional<std::string> FileTool::doWrite(const fs::path& p,
                                              const std::string& content,
                                              bool append) const {
    if (content.size() > max_file_size_bytes_) {
        return std::string("Loi: noi dung qua lon (") +
               std::to_string(content.size()) + " bytes), vuot gioi han " +
               std::to_string(max_file_size_bytes_) + " bytes.";
    }

    std::error_code ec;
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path(), ec);
        // Bo qua loi tao thu muc cha o day: neu that su co van de, thao tac
        // mo file ben duoi se tu bao loi ro rang hon.
    }

    auto mode = std::ios::binary | (append ? std::ios::app : std::ios::trunc);
    std::ofstream ofs(p, mode);
    if (!ofs) {
        return std::string("Loi: khong mo duoc file de ghi -> ") + p.string();
    }
    ofs << content;
    if (!ofs) {
        return std::string("Loi: ghi file that bai -> ") + p.string();
    }

    return std::string(append ? "Da noi " : "Da ghi ") +
           std::to_string(content.size()) + " bytes vao '" + p.string() + "'.";
}

std::optional<std::string> FileTool::doList(const fs::path& p) const {
    std::error_code ec;
    if (!fs::exists(p, ec) || ec) {
        return std::string("Loi: thu muc khong ton tai -> ") + p.string();
    }
    if (!fs::is_directory(p, ec)) {
        return std::string("Loi: '") + p.string() + "' khong phai thu muc.";
    }

    std::ostringstream oss;
    for (const auto& entry : fs::directory_iterator(p, ec)) {
        oss << (entry.is_directory() ? "[DIR]  " : "[FILE] ")
            << entry.path().filename().string() << "\n";
    }
    if (ec) {
        return std::string("Loi: khong doc duoc thu muc -> ") + p.string();
    }

    std::string result = oss.str();
    return result.empty() ? "(thu muc rong)" : result;
}

std::optional<std::string> FileTool::doDelete(const fs::path& p) const {
    std::error_code ec;
    if (!fs::exists(p, ec) || ec) {
        return std::string("Loi: khong ton tai -> ") + p.string();
    }

    auto count = fs::remove_all(p, ec);
    if (ec) {
        return std::string("Loi: xoa that bai -> ") + p.string();
    }
    return std::string("Da xoa ") + std::to_string(count) +
           " muc tai '" + p.string() + "'.";
}

std::optional<std::string> FileTool::doMkdir(const fs::path& p) const {
    std::error_code ec;
    if (fs::exists(p, ec)) {
        if (fs::is_directory(p, ec)) {
            return std::string("Thu muc da ton tai -> ") + p.string();
        }
        return std::string("Loi: '") + p.string() +
               "' da ton tai va khong phai thu muc.";
    }

    bool created = fs::create_directories(p, ec);
    if (ec || !created) {
        return std::string("Loi: tao thu muc that bai -> ") + p.string();
    }
    return std::string("Da tao thu muc '") + p.string() + "'.";
}

std::optional<std::string> FileTool::execute(const std::string& args) {
    json j;
    try {
        j = json::parse(args);
    } catch (const json::parse_error& e) {
        return std::string("Loi parse JSON dau vao: ") + e.what();
    }

    if (!j.contains("action") || !j["action"].is_string()) {
        return std::string("Loi: JSON args thieu truong 'action' kieu string.");
    }
    std::string action = j["action"].get<std::string>();

    // "path" la tuy chon cho action "list" (mac dinh = root workspace).
    std::string rel_path;
    if (j.contains("path") && j["path"].is_string()) {
        rel_path = j["path"].get<std::string>();
    } else if (action != "list") {
        return std::string("Loi: JSON args thieu truong 'path' kieu string.");
    }

    std::string workspace = path_provider_ ? path_provider_() : "";
    if (workspace.empty()) {
        return std::string("Loi: workspace chua duoc khoi tao (Environment "
                            "chua goi setup()).");
    }

    fs::path root(workspace);
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return std::string("Loi: workspace root khong hop le -> ") + workspace;
    }

    auto safe_path = resolveSafePath(root, rel_path);
    if (!safe_path.has_value()) {
        return std::string(
            "Loi: 'path' khong hop le hoac co gang thoat ra ngoai workspace "
            "(chan tuyet doi/'..': ") + rel_path + ").";
    }

    try {
        if (action == "read") {
            return doRead(*safe_path);
        } else if (action == "write" || action == "append") {
            if (!j.contains("content") || !j["content"].is_string()) {
                return std::string(
                    "Loi: action '" + action +
                    "' can truong 'content' kieu string.");
            }
            return doWrite(*safe_path, j["content"].get<std::string>(),
                            action == "append");
        } else if (action == "list") {
            return doList(*safe_path);
        } else if (action == "delete") {
            return doDelete(*safe_path);
        } else if (action == "mkdir") {
            return doMkdir(*safe_path);
        }
        return std::string("Loi: action khong duoc ho tro -> ") + action +
               " (chi ho tro: read, write, append, mkdir, list, delete).";
    } catch (const std::exception& e) {
        return std::string("Loi thuc thi FileTool: ") + e.what();
    }
}