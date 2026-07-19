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