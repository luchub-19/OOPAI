#include "evaluator.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <sys/wait.h>

int FunctionalEvaluator::runScript(const std::string& cmd,
                                    const std::string& traj_path) const {
    std::string full_cmd = cmd + " " + traj_path;
    int ret = std::system(full_cmd.c_str());

    if (ret == -1) {
        std::cerr << "[FunctionalEval] system() failed to fork.\n";
        return -1;
    }
    if (!WIFEXITED(ret)) {
        std::cerr << "[FunctionalEval] Script terminated by signal.\n";
        return -1;
    }
    return WEXITSTATUS(ret);
}

std::optional<float> FunctionalEvaluator::evaluate(const Trajectory& traj,
                                                     const Environment& /*env*/,
                                                     const Task&        task) {
    if (task.eval_script.empty()) {
        std::cerr << "[FunctionalEval] eval_script bi trong -> khong the cham diem.\n";
        return std::nullopt;   // [SỬA]: trước trả 0.0f
    }

    std::string tmp_path = "/tmp/eval_" + traj.task_id + ".json";
    {
        std::ofstream ofs(tmp_path);
        if (!ofs) {
            std::cerr << "[FunctionalEval] Khong the tao file tam: " << tmp_path << "\n";
            return std::nullopt;   // [SỬA]: trước trả 0.0f — lỗi ghi file không phải agent fail
        }
        ofs << traj.exportToJson();
    }

    int exit_code = runScript(task.eval_script, tmp_path);
    std::remove(tmp_path.c_str());

    // [SỬA]: exit_code == -1 nghĩa là system() tự nó lỗi (fork fail / bị
    // signal kill) — đây là lỗi hạ tầng của script checker, KHÔNG phải
    // agent làm sai. Trước đây exit_code == -1 vẫn rơi vào nhánh
    // "score = (exit_code == 0) ? 1.0 : 0.0" -> bị tính oan thành 0.0f
    // y hệt như agent fail thật sự. Giờ tách riêng thành nullopt.
    if (exit_code == -1) {
        return std::nullopt;
    }

    float score = (exit_code == 0) ? 1.0f : 0.0f;
    std::cout << "[FunctionalEval] exit code: " << exit_code
              << "  \u2192  score: " << score << "\n";
    return score;
}