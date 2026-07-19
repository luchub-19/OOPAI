#include "src/tools/web_search_tool.h"
#include <iostream>

int main() {
    WebSearchTool tool("http://localhost:8080", 3, 5);

    std::cout << "Ten Tool: " << tool.getName() << "\n";
    std::cout << "Mo ta:\n\n" << tool.getDescription() << "\n\n";

    std::cout << "=== Test tim kiem tieng Viet co dau ===\n";
    auto r1 = tool.execute(R"({"query": "học máy tính OOP là gì", "num_results": 2})");
    std::cout << (r1 ? *r1 : "(nullopt)") << "\n\n";

    std::cout << "=== Test khong co num_results (dung mac dinh) ===\n";
    auto r2 = tool.execute(R"({"query": "test"})");
    std::cout << (r2 ? *r2 : "(nullopt)") << "\n\n";

    std::cout << "=== Test JSON loi ===\n";
    auto r3 = tool.execute(R"(khong phai json)");
    std::cout << (r3 ? *r3 : "(nullopt)") << "\n\n";

    std::cout << "=== Test thieu query ===\n";
    auto r4 = tool.execute(R"({"num_results": 2})");
    std::cout << (r4 ? *r4 : "(nullopt)") << "\n\n";

    std::cout << "=== Test khong ket noi duoc (sai URL) ===\n";
    WebSearchTool bad_tool("http://localhost:9999", 3, 2);
    auto r5 = bad_tool.execute(R"({"query": "test"})");
    std::cout << (r5 ? *r5 : "(nullopt)") << "\n\n";

    return 0;
}