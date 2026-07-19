// ════════════════════════════════════════════════════════════════
//  benchmark/run_eval.cpp
//
//  Entry point CHO BENCHMARK: doc benchmark/tasks.json -> dung
//  ToolRegistry + OllamaClient + ReActAgentLoop + HarnessRunner (da
//  code san trong src/) de chay tung task -> cham diem bang
//  KeywordEvaluator/FunctionalEvaluator -> xuat trajectory + summary
//  ra file JSON trong benchmark/results/.
//
//  Day KHONG phai noi dinh nghia lai logic harness (HarnessRunner,
//  Evaluator, Environment... da hoan thien trong src/harness/) - file
//  nay chi la "wiring": lap rap cac lop da co lai voi nhau roi chay
//  batch, dung dung vai tro cua benchmark/run_eval.cpp trong cay thu
//  muc de bai (muc VI).
//
//  ------------------------------------------------------------------
//  HIEN TRANG TOOL (quan trong khi doc code nay):
//  Chi CalculatorTool la da code xong (exec/file/web/memory_tool.cpp
//  con rong). Vi vay tasks.json HIEN TAI chi dung calculator, nhung
//  duoc thiet ke de MO RONG de dang: khi mot tool moi xong, chi can:
//    1. #include header tool do + registerTool() them 1 dong trong
//       phan "Tool registry" ben duoi.
//    2. Them task moi trong benchmark/tasks.json dung tool do.
//  KHONG can sua HarnessRunner/Evaluator/AgentLoop vi cac lop nay da
//  duoc thiet ke tach biet dung tinh than de bai (muc 4.4).
//
//  ------------------------------------------------------------------
//  CACH CHAY (sau khi build bang CMake, xem README):
//    ./build/run_eval
//    ./build/run_eval --tasks=benchmark/tasks.json --out=benchmark/results
//    ./build/run_eval --model=qwen3:8b --base-url=http://localhost:11434
//    ./build/run_eval --env=native      # dung NativeEnvironment thay Sandbox
//    ./build/run_eval --tools=calculator  # gioi han tool duoc phep goi
//
//  Bien moi truong OLLAMA_BASE_URL / OLLAMA_MODEL cung duoc doc lam
//  gia tri mac dinh (tham so dong lenh --base-url/--model se GHI DE
//  bien moi truong neu ca hai cung duoc cung cap).
//
//  YEU CAU: Ollama phai dang chay that (vd `ollama serve` +
//  `ollama pull <model>`), neu khong OllamaClient::chat() se fail
//  network -> agent khong the goi tool dung -> hau het task se fail,
//  day la hanh vi dung (khong phai bug cua benchmark).
// ════════════════════════════════════════════════════════════════

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "src/tools/tool_registry.h"
#include "src/tools/calculator_tool.h"
// TODO(khi hoan thien): them include cac tool con lai o day, VD:
#include "src/tools/exec_tool.h"
// #include "src/tools/file_tool.h"
// #include "src/tools/web_tool.h"
#include "src/tools/memory_tool.h"

#include "src/client/ollama_client.h"
#include "src/client/colab_client.h"
#include "src/agent/react_agent_loop.h"
#include "src/agent/skill_loader.h"
#include "src/agent/loop_detector.h"
#include "src/harness/harness_runner.h"
#include "src/harness/environment.h"
#include "src/harness/evaluator.h"
#include "src/common/task.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

// ── Cau hinh dong lenh / bien moi truong ──────────────────────────
struct CliOptions {
    std::string tasks_path = "benchmark/tasks.json";
    std::string out_dir    = "benchmark/results";
    std::string base_url   = "http://localhost:11434"; //http://localhost:11434
    std::string model      = "gemma4:e4b"; // qwen3-vl:8b 
    float       temperature = 0.0f;
    int         max_tokens  = 2048; // để cao 1 tí (tại nó đang tạo thì bị ngắt) = trước đó là 512 -> ko đủ tokens
    std::string env_mode    = "sandbox"; // "native" | "sandbox"
    std::vector<std::string> allowed_tools; // rong = cho phep tat ca tool da dang ky
};

std::optional<std::string> eatFlagValue(const std::string& arg, const std::string& prefix) {
    if (arg.rfind(prefix, 0) == 0) return arg.substr(prefix.size());
    return std::nullopt;
}

CliOptions parseArgs(int argc, char** argv) {
    CliOptions opt;

    // Bien moi truong lam gia tri mac dinh, tham so dong lenh (neu co)
    // se ghi de ben duoi.
    if (const char* v = std::getenv("OLLAMA_BASE_URL")) opt.base_url = v;
    if (const char* v = std::getenv("OLLAMA_MODEL"))    opt.model    = v;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (auto v = eatFlagValue(arg, "--tasks="))       { opt.tasks_path  = *v; continue; }
        if (auto v = eatFlagValue(arg, "--out="))         { opt.out_dir     = *v; continue; }
        if (auto v = eatFlagValue(arg, "--base-url="))    { opt.base_url    = *v; continue; }
        if (auto v = eatFlagValue(arg, "--model="))       { opt.model       = *v; continue; }
        if (auto v = eatFlagValue(arg, "--env="))         { opt.env_mode    = *v; continue; }
        if (auto v = eatFlagValue(arg, "--temperature=")) { opt.temperature = std::stof(*v); continue; }
        if (auto v = eatFlagValue(arg, "--max-tokens="))  { opt.max_tokens  = std::stoi(*v); continue; }
        if (auto v = eatFlagValue(arg, "--tools=")) {
            std::stringstream ss(*v);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                if (!tok.empty()) opt.allowed_tools.push_back(tok);
            }
            continue;
        }
        std::cerr << "[run_eval] Bo qua tham so khong nhan dien: " << arg << "\n";
    }
    return opt;
}

// ── Doc benchmark/tasks.json -> vector<Task> ──────────────────────
// Field "description" trong tasks.json chi de con nguoi doc hieu muc
// dich task (khong anh huong logic cham diem), nen KHONG dua vao
// struct Task (giu Task gon dung theo UML muc 4.1) ma luu rieng ra 1
// map de in trong bao cao tong ket cuoi cung.
std::vector<Task> loadTasks(const std::string& path,
                             std::map<std::string, std::string>& descriptions_out) {
    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error("Khong mo duoc file tasks: " + path);
    }

    json j;
    try {
        ifs >> j;
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("tasks.json khong phai JSON hop le: ") + e.what());
    }
    if (!j.is_array()) {
        throw std::runtime_error("tasks.json phai la 1 JSON array o cap cao nhat.");
    }

    std::vector<Task> tasks;
    tasks.reserve(j.size());
    for (const auto& item : j) {
        Task t;
        t.id          = item.value("id", "");
        t.instruction = item.value("instruction", "");
        t.eval_type   = item.value("eval_type", "keyword");
        t.eval_script = item.value("eval_script", "");
        t.max_steps   = item.value("max_steps", 10);

        // xét riêng thằng image base64
        std::string temp = item.value("image_base64","");
        if(!t.id.empty()){
            t.images_base64 = temp;
        }

        if (t.id.empty() || t.instruction.empty()) {
            std::cerr << "[run_eval] Bo qua task thieu id/instruction: " << item.dump() << "\n";
            continue;
        }
        descriptions_out[t.id] = item.value("description", "");
        tasks.push_back(std::move(t));
    }
    return tasks;
}

} // namespace

int main(int argc, char** argv) {
    CliOptions opt = parseArgs(argc, argv);

    // === 1. Doc tap task ===
    std::map<std::string, std::string> descriptions;
    std::vector<Task> tasks;
    try {
        tasks = loadTasks(opt.tasks_path, descriptions);
    } catch (const std::exception& e) {
        std::cerr << "[run_eval] Loi doc tasks.json: " << e.what() << "\n";
        return 1;
    }
    if (tasks.empty()) {
        std::cerr << "[run_eval] Khong co task hop le nao trong " << opt.tasks_path << "\n";
        return 1;
    }
    std::cout << "[run_eval] Da nap " << tasks.size() << " task tu " << opt.tasks_path << "\n";
    
    // === 2. Environment ===
    // Mac dinh dung SandboxEnvironment (thu muc /tmp/ai_agent_bench_XXXXXX,
    // co lap, tu xoa sau moi task) dung tinh than benchmark khong de lai
    // side-effect. Co the doi sang NativeEnvironment bang --env=native
    // khi can debug va xem workspace that.
    std::unique_ptr<Environment> env;
    if (opt.env_mode == "native") {
        env = std::make_unique<NativeEnvironment>("benchmark_workspace");
    } else {
        env = std::make_unique<SandboxEnvironment>("ai_agent_bench_");
    }

    // === 3. Tool registry ===
    // Hien tai chi CalculatorTool hoan thien. Xem TODO o dau file de
    // biet cach them tool moi khi exec/file/web/memory tool xong.

    // std::string path_of_env = env->getWorkspace(); 

    auto tools = std::make_shared<ToolRegistry>();
    tools->registerTool(std::make_unique<CalculatorTool>());
    tools->registerTool(std::make_unique<MemoryTool>());
    tools->registerTool(std::make_unique<ExecTool>([&env]() { return env->getWorkspace(); },2,false));
    if (!opt.allowed_tools.empty()) {
        tools->setAllowedTools(opt.allowed_tools);
    }

    // === 4. LLM client cho agent ===
    auto llm = std::make_unique<OllamaClient>(opt.base_url, opt.model, opt.temperature, opt.max_tokens);

    // === 5. Skill loader (skills/*.md se duoc inject vao system prompt) ===
    auto skills = std::make_unique<SkillLoader>("skills");
    skills->loadSkillsFromDisk();

    // === 6. Loop detector: canh bao sau 2 lan lap, dung han sau 3 lan ===
    auto loop_detector = std::make_unique<LoopDetector>(2, 3);

    // === 7. Agent loop (ReAct text-only) ===
    ReActAgentLoop agent(std::move(llm), tools, std::move(skills), std::move(loop_detector));


    // === 8. HarnessRunner + dang ky evaluator theo eval_type ===
    HarnessRunner harness(std::move(env));
    harness.registerEvaluator("keyword", std::make_unique<KeywordEvaluator>());
    harness.registerEvaluator("functional", std::make_unique<FunctionalEvaluator>());
    // VLMEvaluator can 1 LLMClient rieng lam "giam khao". tasks.json hien
    // tai khong co task nao dung eval_type="vlm", nhung van dang ky san
    // de benchmark khong bi loi khi ai do them task loai nay sau nay.
    harness.registerEvaluator("vlm", std::make_unique<VLMEvaluator>(
        std::make_unique<OllamaClient>(opt.base_url, opt.model, 0.0f, 256)));

    // === 9. Chay batch ===
    harness.runBatch(tasks, agent);

    // === 10. Xuat ket qua ra JSON (muc 7.1 trajectory + 7.3 success rate) ===
    fs::create_directories(opt.out_dir);

    int   total = 0, succeeded = 0, scored = 0, crashed = 0;
    float score_sum = 0.0f;
    json  summary_tasks = json::array();

    for (const auto& r : harness.getResults()) {
        ++total;
        if (r.agent_crashed) ++crashed;
        if (r.task_success)  ++succeeded;
        if (r.score) { ++scored; score_sum += *r.score; }

        // Ghi trajectory rieng cho tung task -> trajectory_<task_id>.json
        // dung dinh dang goi y o muc 7.1 cua de bai.
        std::string traj_filename = "trajectory_" + r.task_id + ".json";
        std::string traj_path     = opt.out_dir + "/" + traj_filename;
        std::ofstream ofs(traj_path);
        if (ofs) {
            ofs << r.trajectory.exportToJson();
        } else {
            std::cerr << "[run_eval] Khong ghi duoc " << traj_path << "\n";
        }

        summary_tasks.push_back({
            {"task_id",            r.task_id},
            {"description",        descriptions.count(r.task_id) ? descriptions.at(r.task_id) : ""},
            {"crashed",            r.agent_crashed},
            {"trajectory_success", r.trajectory.success},
            {"score",              r.score ? json(*r.score) : json(nullptr)},
            {"task_success",       r.task_success},
            {"trajectory_file",    traj_filename}
        });
    }

    float success_rate = (total > 0)
        ? static_cast<float>(succeeded) / static_cast<float>(total)
        : 0.0f;

    json summary = {
        {"model",           opt.model},
        {"base_url",        opt.base_url},
        {"total_tasks",     total},
        {"crashed_tasks",   crashed},
        {"scored_tasks",    scored},
        {"succeeded_tasks", succeeded},
        {"success_rate",    success_rate},
        {"average_score",   scored > 0 ? json(score_sum / static_cast<float>(scored)) : json(nullptr)},
        {"tasks",           summary_tasks}
    };

    std::string summary_path = opt.out_dir + "/summary.json";
    std::ofstream ofs(summary_path);
    if (ofs) {
        ofs << summary.dump(4);
    } else {
        std::cerr << "[run_eval] Khong ghi duoc " << summary_path << "\n";
    }

    std::cout << "\n========================================\n"
              << "[run_eval] Success rate: " << (success_rate * 100.0f) << "% ("
              << succeeded << "/" << total << " task)\n"
              << "[run_eval] Ket qua tong hop: " << summary_path << "\n"
              << "[run_eval] Trajectory tung task: " << opt.out_dir << "/trajectory_<task_id>.json\n"
              << "========================================\n";

    return 0;
}