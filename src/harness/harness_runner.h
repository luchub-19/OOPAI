#pragma once
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <stdexcept>

#include "src/harness/environment.h"
#include "src/harness/evaluator.h"
#include "src/harness/trajectory.h"
#include "src/agent/agent_loop.h"
#include "src/common/task.h"

// ════════════════════════════════════════════════════════════════
//  HarnessRunner — orchestrator tầng trên cùng.
//  KHÔNG tự chạy ReAct loop (việc của AgentLoop), KHÔNG tự chấm điểm
//  (việc của Evaluator), KHÔNG tự tạo workspace (việc của Environment).
//  HarnessRunner chỉ ĐIỀU PHỐI trình tự:
//    setup env -> runAgent -> chọn evaluator theo Task::eval_type
//    -> evaluate -> teardown env -> lặp cho task kế tiếp.
// ════════════════════════════════════════════════════════════════
class HarnessRunner {
public:
    // Kết quả 1 task sau khi chạy xong pipeline — bổ sung so với UML gốc
    // (UML chỉ ghi runBatch() trả void, nhưng void không lưu gì thì
    // không tổng kết batch được).
    struct BatchResult {
        std::string task_id;
        bool        agent_crashed = false;
        // optional<float>: nullopt = "không chấm được" (thiếu evaluator
        // khớp eval_type, hoặc evaluator chạy nhưng không ra điểm được -
        // VD FunctionalEvaluator script lỗi, VLMEvaluator model không trả
        // lời). 0.0f = chấm được, agent fail thật. Không dùng cờ riêng
        // "evaluator_found" vì optional đã tự mang đủ thông tin đó.
        std::optional<float> score;
        Trajectory  trajectory;
        // [MỚI] "task_success" khác hẳn "trajectory.success":
        //   - trajectory.success (set trong AgentLoop::run()) chỉ nghĩa là
        //     agent thoát loop bằng "Final Answer" đúng cú pháp, KHÔNG phản
        //     ánh câu trả lời đúng hay sai.
        //   - task_success ở đây mới là "task có đạt yêu cầu hay không",
        //     tính từ score sau khi evaluator chấm xong. Tách riêng 2 khái
        //     niệm để tránh nhầm lẫn kiểu score=0.33 nhưng in ra success=true.
        bool task_success = false;
    };

    explicit HarnessRunner(std::unique_ptr<Environment> env);

    // Đăng ký 1 evaluator gắn với 1 type_key khớp Task::eval_type
    // (VD: "keyword", "functional", "vlm").
    void registerEvaluator(std::string type_key, std::unique_ptr<Evaluator> evaluator);

    // Chạy 1 agent trên 1 task, trả Trajectory thô — KHÔNG setup/teardown
    // env, KHÔNG chấm điểm. Public để có thể test/debug 1 task đơn lẻ
    // mà không cần chạy nguyên batch.
    Trajectory runAgent(AgentLoop& loop, const Task& task);

    // Chạy pipeline đầy đủ cho nhiều task: setup -> runAgent -> evaluate
    // -> teardown, lặp cho từng task, in báo cáo tổng kết cuối cùng.
    void runBatch(const std::vector<Task>& tasks, AgentLoop& loop);

    [[nodiscard]] const std::vector<BatchResult>& getResults() const { return results_; }

private:
    struct EvaluatorEntry {
        std::string type_key;
        std::unique_ptr<Evaluator> evaluator;
    };

    void onStepRecorded(Step step);
    void printBatchSummary() const;

    std::unique_ptr<Environment> env_;
    std::vector<EvaluatorEntry>  evaluators_;
    std::vector<BatchResult>     results_;
    std::string                  current_task_id_;  // context cho onStepRecorded
};