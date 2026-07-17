#include <iostream>
#include <string>
#include "src/tools/calculator_tool.h"
#include "agent/skill_loader.h"
#include "agent/loop_detector.h"
#include "agent/agent_loop.h"
#include "src/agent/react_agent_loop.h"

int main() {
    // =================================================================
    // 1. CẤU HÌNH ENVIRONMENT & LLM CLIENT (BẮT BUỘC DÙNG MODEL VISION)
    // =================================================================
    // CẢNH BÁO: Phải dùng model hỗ trợ ảnh (VD: qwen3-vl:7b hoặc llava:7b). 
    // Nếu dùng gemma4 text-only sẽ bị lỗi 400.
    const char* model_env = std::getenv("OLLAMA_MODEL");
    std::string model_name = (model_env && *model_env) ? model_env : "gemma4:e4b"; // qwen3-vl:8b

    // CHÚ Ý TĂNG TIMEOUT: Đã set max_tokens=1024. Đảm bảo trong ollama_client.cpp 
    // bạn đã tăng CURLOPT_TIMEOUT lên ít nhất 180L để Colab có thời gian đọc ảnh.
    auto llm = std::make_unique<OllamaClient>("localhost:11434", model_name, 0.0f, 1024);
    
    // =================================================================
    // 2. KHỞI TẠO CÁC MODULE CỦA HỆ THỐNG AGENT (SKILL, TOOL, LOOP)
    // =================================================================
    auto reg = std::make_shared<ToolRegistry>();
    // Đăng ký Tool tính toán để Agent sử dụng sau khi đọc được ảnh
    reg->registerTool(std::make_unique<CalculatorTool>());

    auto skills = std::make_unique<SkillLoader>("skills");
    // Cấu hình LoopDetector: 2 vòng lặp giống nhau là cảnh báo, 3 là dừng
    auto loop_det = std::make_unique<LoopDetector>(2, 3);

    // Bơm các thành phần vào Agent Loop
    ReActAgentLoop agent(std::move(llm), reg, std::move(skills), std::move(loop_det));

    // =================================================================
    // 3. TẠO TASK CHỨA ẢNH (MULTIMODAL TASK)
    // =================================================================
    Task t;
    t.id = "test_vision_agent_01";
    // System prompt/Instruction hướng dẫn Agent phải làm gì
    // t.instruction = "tính toán biểu thức ((3^2 - 5 + 2) / sqrt(16)) + sin(pi/6) - cos(pi/3) + tan(pi/4) + ln(e^3) + log(1000) + sqrt(25 + 9) - (2 * 5 - 1)^3 / 8 + (5 * 2 + 7)^2 / (8 - 2) ?"; 
    t.instruction = "hãy đọc bức ảnh và phân tích bức ảnh này nói đúng hay sai";
    t.max_steps = 5;

    // Chuỗi Base64 chuẩn PNG do bạn cung cấp
    // std::string user_base64_image = "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";
    
    // Gắn ảnh vào struct Task (Đảm bảo struct Task của bạn có trường vector này)
    // t.images_base64 =  user_base64_image ;

int main(){
    std::cout << " === calculator tool === \n ";

    std::string promts = "((3^2 - 5 + 2) / \\sqrt{16}) + \\sin(\\frac{\\pi}{6}) - \\cos(\\frac{\\pi}{3}) + \\tan(\\frac{\\pi}{4}) + \\ln(e^3)";

    CalculatorTool caltool;
    std::string test = (caltool.execute(promts)).value();
    std::cout << "test: " << test << std::endl;
    return 0;
}