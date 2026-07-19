#pragma once
#include <string>

// ════════════════════════════════════════════════════════════════
//  Abstract Environment  (Strategy: chọn môi trường thực thi)
// ════════════════════════════════════════════════════════════════

class Environment {
public:
    virtual ~Environment() = default;
    virtual void setup()    = 0;   // Gọi trước khi agent bắt đầu chạy
    virtual void teardown() = 0;   // Gọi sau khi eval xong để dọn dẹp

    const virtual std::string& getWorkspace() const = 0;
};

// ── NativeEnvironment ─────────────────────────────────────────
// Chạy trực tiếp trên OS host.
// Workspace được tạo tại thư mục có tên cố định (do user truyền vào).
// Phù hợp cho dev/testing khi không cần cô lập.
class NativeEnvironment : public Environment {
    std::string work_dir_;
public:
    explicit NativeEnvironment(const std::string& dir = "agent_workspace");
    void setup()    override;
    void teardown() override;
    const std::string& getWorkspace() const override { return work_dir_; }
};

// ── SandboxEnvironment ────────────────────────────────────────
// Workspace cô lập trong /tmp với tên được sinh ngẫu nhiên bởi mkdtemp.
// Phù hợp cho benchmark/eval để tránh side-effect giữa các lần chạy.
class SandboxEnvironment : public Environment {
    std::string prefix_;
    std::string sandbox_dir_;
    bool        active_ = false;
public:
    explicit SandboxEnvironment(const std::string& prefix = "ai_agent_");
    void setup()    override;
    void teardown() override;
    const std::string& getWorkspace() const override { return sandbox_dir_; }
};
