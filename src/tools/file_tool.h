#pragma once

#include "src/tools/tool.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

// FileTool: tao va thao tac file/folder ben trong workspace cua agent.
// Args dau vao la JSON:
//   {"action": "read",   "path": "duong/dan/tuong/doi"}
//   {"action": "write",  "path": "...", "content": "..."}   // tao moi / ghi de
//   {"action": "append", "path": "...", "content": "..."}   // noi vao cuoi file
//   {"action": "mkdir",  "path": "..."}
//   {"action": "list",   "path": "..."}                     // "" hoac "." = liet ke root
//   {"action": "delete", "path": "..."}                     // xoa file hoac thu muc (de quy)
//
// Day la 1 trong 5 tool bat buoc (muc 3.2 de bai).
//
// path_provider_: giong ExecTool, tra ve workspace root hien tai (co the doi
//   khi Environment tao workspace sau khi tool duoc khoi tao, vi du
//   SandboxEnvironment sinh thu muc bang mkdtemp trong setup()).
class FileTool : public Tool {
public:
    // maxFileSizeBytes: gioi han kich thuoc file duoc doc/ghi, tranh agent
    //   lam tran bo nho / spam workspace voi file khong lo.
    explicit FileTool(std::function<std::string()> pathProvider,
                       std::size_t maxFileSizeBytes = 5 * 1024 * 1024);

    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] std::string getDescription() const override;
    std::optional<std::string> execute(const std::string& args) override;

private:
    std::function<std::string()> path_provider_;
    std::size_t max_file_size_bytes_;

    // Ghep relative_path voi workspace root hien tai, chuan hoa (weakly_canonical)
    // va kiem tra ket qua van nam TRONG root -> chong path traversal (../, path
    // tuyet doi, symlink thoat ra ngoai). Tra ve nullopt neu khong hop le.
    std::optional<std::filesystem::path> resolveSafePath(
        const std::filesystem::path& root,
        const std::string& relative_path) const;

    std::optional<std::string> doRead(const std::filesystem::path& p) const;
    std::optional<std::string> doWrite(const std::filesystem::path& p,
                                        const std::string& content,
                                        bool append) const;
    std::optional<std::string> doList(const std::filesystem::path& p) const;
    std::optional<std::string> doDelete(const std::filesystem::path& p) const;
    std::optional<std::string> doMkdir(const std::filesystem::path& p) const;
};