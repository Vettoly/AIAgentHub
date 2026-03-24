#pragma once
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp> // 🌟 必须引入 json 头文件

class LLMEngine {
public:
    static void Init();
    static void Cleanup();
    static std::string CallQwen(const std::string& user_message, const std::string& api_key);
    
    static void CallQwenStream(const std::string& user_message, const std::string& api_key, std::function<void(const std::string&)> stream_callback);

    // 🌟 带记忆的流式调用
    static void CallQwenStreamWithMemory(
        const std::vector<nlohmann::json>& history, // 传入从数据库捞出的记忆
        const std::string& current_prompt,          // 当前最新的问题
        const std::string& api_key, 
        std::function<void(const std::string&)> stream_callback);

    static std::vector<float> GetEmbedding(const std::string& text, const std::string& api_key);
    
private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    static size_t StreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
};