// #include <iostream>
// #include <string>
// #include "src/tools/exec_tool.h" // Chỉnh lại đường dẫn include cho khớp với code của bạn

// // Hàm helper để in kết quả cho đẹp
// void printTestResult(const std::string& test_name, const std::optional<std::string>& result) {
//     std::cout << "==================================================\n";
//     std::cout << "[TEST] " << test_name << "\n";
//     if (result.has_value()) {
//         std::cout << ">> KẾT QUẢ:\n" << result.value() << "\n";
//     } else {
//         std::cout << ">> KẾT QUẢ: [Thất bại / std::nullopt]\n";
//     }
//     std::cout << "==================================================\n\n";
// }

// int main() {
//     std::cout << "BẮT ĐẦU TEST EXEC TOOL...\n\n";

//     // Khởi tạo tool với timeout 2 giây và KHÔNG cho phép shell meta characters
//     ExecTool exec_tool("/run\\media\\raphael\\Partition_D\\education\term 3\\OOP\\Project_OOP", 2, false);

//     // 1. Kiểm tra thông tin
//     std::cout << "Tên Tool: " << exec_tool.getName() << "\n";
//     std::cout << "Mô tả:\n" << exec_tool.getDescription() << "\n\n";

//     // 2. Test lệnh an toàn cơ bản (Dùng echo - chạy được trên cả Linux và Windows)
//     std::string args_echo = R"({"command": "echo Hello AI Agent"})";
//     printTestResult("1. Lệnh cơ bản (echo)", exec_tool.execute(args_echo));

//     // 3. Test lệnh liệt kê thư mục (Dùng lệnh ls của Linux)
//     // Lưu ý: Nếu bạn test trên Windows, hãy đổi "ls -la" thành "dir"
//     std::string args_ls = R"({"command": "ls -la"})";
//     printTestResult("2. Lệnh liệt kê thư mục (ls -la)", exec_tool.execute(args_ls));

//     // 4. Test chức năng Blocklist (Ngăn chặn lệnh nguy hiểm)
//     std::string args_rm = R"({"command": "rm -rf / thư_mục_quan_trọng"})";
//     printTestResult("3. Chặn lệnh nguy hiểm (rm -rf)", exec_tool.execute(args_rm));

//     // 5. Test chức năng Blocklist (Ngăn chặn trình soạn thảo tương tác)
//     std::string args_nano = R"({"command": "nano test.txt"})";
//     printTestResult("4. Chặn lệnh tương tác (nano)", exec_tool.execute(args_nano));

//     // 6. Test cơ chế Timeout
//     // Lệnh sleep 5 giây, nhưng tool chỉ cho phép 2 giây. 
//     // (Trên Windows không có lệnh sleep, bạn có thể đổi thành "ping -n 6 127.0.0.1")
//     std::string args_sleep = R"({"command": "sleep 15"})";
//     printTestResult("5. Cơ chế Timeout (Quá 2 giây)", exec_tool.execute(args_sleep));

//     // 7. Test xử lý JSON lỗi (Robustness)
//     std::string args_bad_json = R"({command: "echo Thiếu ngoặc kép ở key"})";
//     printTestResult("6. Xử lý lỗi JSON sai cú pháp", exec_tool.execute(args_bad_json));

//     // 8. Test chạy lệnh thô (Fallback nếu AI quên truyền JSON)
//     std::string args_raw = "echo Day la chuoi tho khong phai JSON";
//     printTestResult("7. Khả năng chịu lỗi (Fallback chuỗi thô)", exec_tool.execute(args_raw));

//     std::cout << "HOÀN THÀNH TEST!\n";
//     return 0;
// }

// =====================================================================
// main.cpp — SELF-TEST cho target "AI_Agent"
//
// File nay TRUOC DAY chi la code test tay cho ExecTool, bi comment het
// va KHONG CO ham main() -> linker bao "undefined reference to `main'"
// khi build target AI_Agent (run_eval van build binh thuong vi no dung
// benchmark/run_eval.cpp, co main() rieng).
//
// Viet lai thanh 1 bo self-test nho, chay doc lap khong can Ollama/mang,
// kiem tra LAI dung nhung cho vua sua bug de tranh regression ve sau:
//
//   [Bug #1] Trajectory::exportToJson() - std::optional<string> khong
//            serialize duoc truc tiep qua nlohmann::json (compile error)
//   [Bug #2] benchmark/run_eval.cpp - dangling reference sau std::move(env)
//            (khong test duoc tu main.cpp vi nam trong 1 file .cpp khac,
//            chi ghi chu lai o day, xem lai run_eval.cpp de doi chieu)
//   [Bug A]  MemoryTool - crash (json::type_error khong duoc bat) khi
//            LLM sinh "action"/"data" khong phai string
//   [Bug B]  WebSearchTool::formatResults() - crash (json::type_error)
//            khi server tra ve field sai kieu
//   [Bug C]  FileTool - action "delete" voi path="." xoa mat ca workspace
//            root thay vi chi cho xoa file/thu muc con
//
// Khong dung framework test ngoai (gtest...) vi de bai khong yeu cau -
// chi 1 ham check() dem so PASS/FAIL, khong dung assert() vi assert()
// sap chuong trinh ngay khi FAIL dau tien, con o day muon chay HET moi
// test roi moi tong ket.
// =====================================================================

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

#include "src/harness/trajectory.h"
#include "src/common/step.h"
#include "src/tools/file_tool.h"
#include "src/tools/memory_tool.h"
#include "src/tools/web_search_tool.h"
#include "src/tools/calculator_tool.h"
#include "src/tools/exec_tool.h"

namespace fs = std::filesystem;

static int g_pass = 0;
static int g_fail = 0;

void check(bool condition, const std::string& label) {
    if (condition) {
        std::cout << "  [PASS] " << label << "\n";
        ++g_pass;
    } else {
        std::cout << "  [FAIL] " << label << "\n";
        ++g_fail;
    }
}

void section(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

// ---------------------------------------------------------------------
// [Bug #1] Trajectory::exportToJson() phai serialize duoc Step co
// tool_result la std::optional<std::string>, ca truong hop co gia tri
// lan nullopt (truoc day nullopt/co gia tri deu khien code KHONG compile
// duoc, nen o day chi can chay toi day va khong throw la da chung minh
// duoc fix hoat dong dung).
// ---------------------------------------------------------------------
void test_trajectory_export() {
    section("[Bug #1] Trajectory::exportToJson() voi optional<string>");

    Trajectory traj;
    traj.task_id = "selftest_task";
    traj.model = "selftest_model";
    traj.success = true;
    traj.total_time_ms = 123;

    Step s1;
    s1.step_id = 1;
    s1.thought = "co tool_result";
    s1.action_type = "tool_call";
    s1.tool_name = "calculator";
    s1.tool_args = "1+1";
    s1.tool_result = "2";              // CO gia tri
    s1.tokens_used = 10;
    s1.latency_ms = 5;
    traj.steps.push_back(s1);

    Step s2;
    s2.step_id = 2;
    s2.thought = "khong co tool_result (vi du buoc bi loi truoc khi tool chay)";
    s2.action_type = "error";
    s2.tool_result = std::nullopt;     // NULLOPT - truong hop hay bi bo sot
    s2.tokens_used = 0;
    s2.latency_ms = 0;
    traj.steps.push_back(s2);

    try {
        std::string json_str = traj.exportToJson();
        check(!json_str.empty(), "exportToJson() chay xong, khong throw");
        check(json_str.find("\"tool_result\":\"2\"") != std::string::npos ||
                  json_str.find("\"tool_result\": \"2\"") != std::string::npos,
              "Step co tool_result duoc serialize dung gia tri");
        check(json_str.find("\"tool_result\":null") != std::string::npos ||
                  json_str.find("\"tool_result\": null") != std::string::npos,
              "Step tool_result=nullopt duoc serialize thanh JSON null (khong throw)");
    } catch (const std::exception& e) {
        check(false, std::string("exportToJson() KHONG duoc throw nhung da throw: ") + e.what());
    }
}

// ---------------------------------------------------------------------
// [Bug A] MemoryTool khong duoc crash khi "action"/"data" sai kieu.
// ---------------------------------------------------------------------
void test_memory_tool() {
    section("[Bug A] MemoryTool - khong crash voi JSON sai kieu");

    MemoryTool tool(":memory:");

    auto r1 = tool.execute(R"({"action":"save","data":"toi thich C++"})");
    check(r1.has_value(), "save() hoat dong binh thuong");

    auto r2 = tool.execute(R"({"action":"search","data":"C++"})");
    check(r2.has_value() && r2->find("C++") != std::string::npos,
          "search() tim lai dung du lieu vua luu");

    // Cac truong hop truoc day lam nem json::type_error khong duoc bat
    auto r3 = tool.execute(R"({"action":123,"data":"x"})");
    check(r3.has_value() && r3->find("string") != std::string::npos,
          "action la so -> tra loi ro rang, khong crash");

    auto r4 = tool.execute(R"({"action":"save","data":123})");
    check(r4.has_value(), "data la so -> tra loi ro rang, khong crash");

    auto r5 = tool.execute(R"({"action":"save","data":null})");
    check(r5.has_value(), "data la null -> tra loi ro rang, khong crash");
}

// ---------------------------------------------------------------------
// [Bug B] WebSearchTool khong duoc crash voi cac nhanh loi co ban.
// (Khong mo server that trong self-test nay de tranh phu thuoc mang -
// phan formatResults() voi field sai kieu da duoc kiem chung rieng luc
// sua bug bang mock server, o day chi test cac nhanh loi tinh khong can
// mang.)
// ---------------------------------------------------------------------
void test_web_search_tool() {
    section("[Bug B] WebSearchTool - cac nhanh loi co ban khong crash");

    WebSearchTool tool_no_url("", 5, 3);
    auto r1 = tool_no_url.execute(R"({"query":"test"})");
    check(r1.has_value(), "search_api_url rong -> tra loi ro rang, khong crash");

    WebSearchTool tool("http://127.0.0.1:1", 5, 1);
    auto r2 = tool.execute(R"({"query":""})");
    check(r2.has_value(), "query rong -> tra loi ro rang, khong crash");

    auto r3 = tool.execute(R"({"q":"sai ten field"})");
    check(r3.has_value(), "thieu field 'query' -> tra loi ro rang, khong crash");
}

// ---------------------------------------------------------------------
// [Bug C] FileTool khong duoc cho xoa chinh workspace root, nhung van
// phai xoa duoc file/thu muc con binh thuong.
// ---------------------------------------------------------------------
void test_file_tool() {
    section("[Bug C] FileTool - khong duoc tu xoa workspace root");

    fs::path ws = fs::temp_directory_path() / "ai_agent_selftest_ws";
    std::error_code ec;
    fs::remove_all(ws, ec);
    fs::create_directories(ws / "subdir", ec);
    {
        std::ofstream(ws / "a.txt") << "hello";
        std::ofstream(ws / "subdir" / "b.txt") << "world";
    }

    std::string ws_str = ws.string();
    FileTool tool([ws_str]() { return ws_str; });

    auto r_del_dot = tool.execute(R"({"action":"delete","path":"."})");
    check(r_del_dot.has_value() && r_del_dot->find("Loi") == 0,
          "delete path=\".\" bi tu choi (khong xoa duoc workspace root)");
    check(fs::exists(ws), "workspace root van con ton tai sau khi thu xoa \".\"");

    auto r_del_empty = tool.execute(R"({"action":"delete","path":""})");
    check(r_del_empty.has_value() && r_del_empty->find("Loi") == 0,
          "delete path=\"\" cung bi tu choi");
    check(fs::exists(ws), "workspace root van con ton tai sau khi thu xoa \"\"");

    auto r_del_file = tool.execute(R"({"action":"delete","path":"a.txt"})");
    check(r_del_file.has_value() && r_del_file->find("Loi") != 0,
          "van xoa duoc 1 file con binh thuong");
    check(!fs::exists(ws / "a.txt"), "a.txt da thuc su bi xoa");

    auto r_del_dir = tool.execute(R"({"action":"delete","path":"subdir"})");
    check(r_del_dir.has_value() && r_del_dir->find("Loi") != 0,
          "van xoa duoc 1 thu muc con (de quy) binh thuong");
    check(!fs::exists(ws / "subdir"), "subdir da thuc su bi xoa");
    check(fs::exists(ws), "workspace root VAN CON sau khi xoa het file/thu muc con");

    // Doi chieu: path traversal / symlink escape (da dung tu truoc, kiem
    // tra lai o day de chac fix Bug C khong lam hong hanh vi cu)
    auto r_abs = tool.execute(R"({"action":"read","path":"/etc/passwd"})");
    check(r_abs.has_value() && r_abs->find("Loi") == 0,
          "(regression check) path tuyet doi van bi chan nhu cu");

    fs::remove_all(ws, ec);
}

// ---------------------------------------------------------------------
// Sanity check nhanh cho 2 tool von da on dinh tu truoc (khong sua gi),
// chay them cho du bo "5 tool bat buoc" duoc dong cham trong self-test.
// ---------------------------------------------------------------------
void test_stable_tools_sanity() {
    section("Sanity check - CalculatorTool & ExecTool (khong doi, van on dinh)");

    CalculatorTool calc;
    auto rc = calc.execute("2+3*4");
    check(rc.has_value() && *rc == "14", "CalculatorTool: 2+3*4 = 14");

    fs::path ws = fs::temp_directory_path() / "ai_agent_selftest_exec_ws";
    std::error_code ec;
    fs::create_directories(ws, ec);
    std::string ws_str = ws.string();
    ExecTool exec_tool([ws_str]() { return ws_str; }, 2, false);
    auto re = exec_tool.execute(R"({"command":"echo hello_from_exec_tool"})");
    check(re.has_value() && re->find("hello_from_exec_tool") != std::string::npos,
          "ExecTool: echo chay dung trong workspace");
    fs::remove_all(ws, ec);
}

int main() {
    std::cout << "==========================================\n";
    std::cout << " AI_Agent SELF-TEST - kiem tra lai cac bug\n";
    std::cout << " da sua (khong can Ollama / mang)\n";
    std::cout << "==========================================\n";

    test_trajectory_export();
    test_memory_tool();
    test_web_search_tool();
    test_file_tool();
    test_stable_tools_sanity();

    std::cout << "\n==========================================\n";
    std::cout << " KET QUA: " << g_pass << " PASS, " << g_fail << " FAIL\n";
    std::cout << "==========================================\n";

    return g_fail == 0 ? 0 : 1;
}