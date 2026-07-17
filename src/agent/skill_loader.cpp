#include "skill_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <unordered_map>
#include <algorithm>

namespace fs = std::filesystem;

// Hàm nhận prompt thô và trả về danh sách từ khóa skill chuẩn
    static std::vector<std::string> extract(const std::string& raw_prompt) {
        // 1. Chuyển prompt sang chữ thường để dễ xử lý
        std::string lower_prompt = raw_prompt;
        std::transform(lower_prompt.begin(), lower_prompt.end(), lower_prompt.begin(), ::tolower);

        std::vector<std::string> matched_skills;

        // 2. Từ điển Mapping: { "dấu hiệu nhận biết", "tên skill chuẩn" }
        // Bạn có thể mở rộng danh sách này tùy theo các skill file bạn viết
        std::unordered_map<std::string, std::string> keyword_map = {
            // Nhóm skill: Tính toán (calculator.md)
            {"tính", "calculator"},
            {"tĩnh", "calculator"}, // Bắt luôn lỗi chính tả thường gặp
            {"toán", "calculator"},
            {"+", "calculator"},
            {"-", "calculator"},
            {"*", "calculator"},
            {"/", "calculator"},
            {"cộng", "calculator"},

            // Nhóm skill: Lên kế hoạch (task_planner.md)
            {"kế hoạch", "task_planner"},
            {"plan", "task_planner"},
            {"bước", "task_planner"},
            {"step", "task_planner"},
            {"phức tạp", "task_planner"},

            // Nhóm skill: Xử lý lỗi (error_recovery.md)
            {"lỗi", "error_recovery"},
            {"error", "error_recovery"},
            {"fail", "error_recovery"}
        };

        // 3. Quét prompt xem có chứa dấu hiệu nào không
        for (const auto& [trigger_word, target_skill] : keyword_map) {
            if (lower_prompt.find(trigger_word) != std::string::npos) {
                // Nếu tìm thấy, kiểm tra xem skill này đã được thêm vào mảng chưa (tránh trùng lặp)
                if (std::find(matched_skills.begin(), matched_skills.end(), target_skill) == matched_skills.end()) {
                    matched_skills.push_back(target_skill);
                }
            }
        }

        return matched_skills;
    }

SkillLoader::SkillLoader(fs::path dir) : skill_dir(std::move(dir)) {}

void SkillLoader::loadSkillsFromDisk() {
    skills.clear();
    if (!fs::exists(skill_dir) || !fs::is_directory(skill_dir)) {
        std::cerr << "[SkillLoader] Cảnh báo: Thư mục skill không tồn tại -> " << skill_dir << "\n";
        return;
    }

    // Kỹ thuật C++17: Duyệt file trong thư mục
    for (const auto& entry : fs::directory_iterator(skill_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md") {
            std::ifstream ifs(entry.path());
            if (ifs) {
                std::ostringstream oss;
                oss << ifs.rdbuf();
                std::string stem_name = entry.path().stem().string(); // Cắt đuôi .md
                skills[stem_name] = oss.str();
            }
        }
    }
}

std::string SkillLoader::selectSkill(const std::string& keyword) const {
    const std::vector<std::string>& keywords = extract(keyword); // keyword != keywords
    if (keywords.empty()) return "";

    std::string accumulated_skills = ""; // Biến cộng dồn các kĩ năng tìm được
    
    // Duyệt qua từng từ khóa (VD: "calculator", "plan")
    for (const auto& kw : keywords) {
        if (kw.empty()) continue;
        
        std::regex word_regex("\\b" + kw + "\\b", std::regex_constants::icase);

        for (const auto& [name, content] : skills) {
            // Kiểm tra xem skill này đã được nạp vào accumulated_skills chưa 
            // (tránh trùng lặp nếu 2 từ khóa cùng trỏ về 1 file)
            if (accumulated_skills.find("[SPECIAL SKILL INSTRUCTION: " + name + "]") != std::string::npos) {
                continue; 
            }

            // Quét tên file và nội dung
            if (std::regex_search(name, word_regex) || std::regex_search(content, word_regex)) {
                accumulated_skills += "\n=== [SPECIAL SKILL INSTRUCTION: " + name + "] ===\n" 
                                    + content 
                                    + "\n===================================\n";
            }
        }
    }

    return accumulated_skills; // Trả về 1 chuỗi dài chứa nhiều kĩ năng
}