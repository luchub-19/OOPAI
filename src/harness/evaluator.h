#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "trajectory.h"
#include "environment.h"
#include "src/common/task.h"
#include "src/client/llm_client.h"   // LLMClient, Message — cần cho VLMEvaluator

// ════════════════════════════════════════════════════════════════
//  Abstract Evaluator  (Strategy Pattern)
//  evaluate() trả std::optional<float> trong [0.0, 1.0]:
//    - nullopt      -> KHÔNG THỂ chấm điểm (thiếu input, script/model lỗi)
//    - 0.0f          -> chấm được, agent fail hoàn toàn
//    - 1.0f          -> chấm được, agent pass hoàn toàn
//  [SỬA LỖI]: bản trước trả float trần, lẫn lộn "không chấm được" với
//  "chấm được và fail" -> làm sai average score ở HarnessRunner khi
//  gom nhiều task (một task lỗi hạ tầng bị tính ngang với một task agent
//  thực sự làm sai). Đây cũng đúng khớp lại với class diagram gốc.
// ════════════════════════════════════════════════════════════════

class Evaluator {
public:
    virtual ~Evaluator() = default;
    virtual std::optional<float> evaluate(const Trajectory& traj,
                                           const Environment& env,
                                           const Task&        task) = 0;
};

// ── KeywordEvaluator ──────────────────────────────────────────
// eval_script format: "result, success, completed" (CSV keyword)
// Score = số_keyword_tìm_thấy / tổng_số_keyword
// nullopt nếu eval_script không parse ra keyword nào (không phải lỗi
// của agent, mà là task setup thiếu sót).
class KeywordEvaluator : public Evaluator {
    std::vector<std::string> parseKeywords(const std::string& script) const;
    bool findKeyword(const std::string& text, const std::string& kw) const;
public:
    std::optional<float> evaluate(const Trajectory& traj,
                                   const Environment& env,
                                   const Task&        task) override;
};

// ── FunctionalEvaluator ───────────────────────────────────────
// eval_script format: lệnh shell hoặc đường dẫn executable
// Framework gọi: <eval_script> <path_to_trajectory.json>
//   Exit code 0   -> score 1.0
//   Exit code ≠ 0 -> score 0.0
//   system() lỗi fork / script bị signal kill -> nullopt (hạ tầng lỗi,
//   không phải agent fail)
class FunctionalEvaluator : public Evaluator {
    // Trả exit code thật, hoặc -1 nếu system() fork lỗi / bị signal
    int runScript(const std::string& cmd, const std::string& traj_path) const;
public:
    std::optional<float> evaluate(const Trajectory& traj,
                                   const Environment& env,
                                   const Task&        task) override;
};

// ── VLMEvaluator ──────────────────────────────────────────────
// "LLM-as-judge": dùng vlm_client (LLMClient bất kỳ, không nhất thiết
// phải multimodal — tên VLM giữ theo class diagram/Bonus 10.1, nhưng
// hiện tại ReActAgentLoop text-only nên Step chưa có ảnh; nếu sau này
// GUIAgentLoop sinh screenshot, Message::images_base64 sẵn có chỗ để
// nhét ảnh vào history dưới đây mà không cần đổi interface).
//
// eval_script format: tiêu chí chấm điểm dạng văn xuôi tự do, VD:
//   "Agent phải tính đúng kết quả phép cộng và trả lời bằng số cụ thể,
//    không được bịa kết quả nếu tool báo lỗi."
// Nếu eval_script rỗng, dùng task.instruction làm tiêu chí mặc định.
//
// Judge được yêu cầu trả lời có dòng "SCORE: <0.0-1.0>" ở cuối, evaluator
// regex ra số đó. nullopt nếu: vlm_client null, chat() trả content rỗng,
// hoặc không tìm được dòng SCORE hợp lệ trong output.
class VLMEvaluator : public Evaluator {
    std::unique_ptr<LLMClient> vlm_client;

    std::string buildJudgePrompt(const Trajectory& traj, const Task& task) const;
    std::optional<float> parseScore(const std::string& judge_response) const;
public:
    explicit VLMEvaluator(std::unique_ptr<LLMClient> client);

    std::optional<float> evaluate(const Trajectory& traj,
                                   const Environment& env,
                                   const Task&        task) override;
};