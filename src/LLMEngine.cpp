#include "LLMEngine.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

void LLMEngine::Init() {
    curl_global_init(CURL_GLOBAL_ALL);
}

void LLMEngine::Cleanup() {
    curl_global_cleanup();
}

size_t LLMEngine::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string LLMEngine::CallQwen(const std::string& user_message, const std::string& api_key) {
    CURL* curl = curl_easy_init();
    if (!curl) return "AI 引擎初始化失败";

    std::string response_string;
    std::string api_url = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";

    json request_body = {
        {"model", "qwen-plus"},
        {"messages", {
            {{"role", "system"}, {"content", "你是一个友好的群聊 AI 助手，请简短回答。"}},
            {{"role", "user"}, {"content", user_message}}
        }}
    };
    std::string body_str = request_body.dump();

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "AI 网络请求失败";
    try {
        json response_json = json::parse(response_string);
        if (response_json.contains("error")) {
            return "🔴 阿里 API 报错: " + response_json["error"]["message"].get<std::string>();
        }
        return response_json["choices"][0]["message"]["content"];
    } catch (...) { return "🔴 收到未知格式数据: " + response_string; }
}

std::vector<float> LLMEngine::GetEmbedding(const std::string& text, const std::string& api_key) {
    std::vector<float> embedding;
    CURL* curl = curl_easy_init();
    if (!curl) return embedding;

    std::string response_string;
    // 🌟 注意：这里调用的是阿里专门用来做向量化的 API
    std::string api_url = "https://dashscope.aliyuncs.com/api/v1/services/embeddings/text-embedding/text-embedding";

    json request_body = {
        {"model", "text-embedding-v1"},
        {"input", {
            {"texts", {text}} // 把我们要转换的句子塞进去
        }}
    };
    std::string body_str = request_body.dump();

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK) {
        try {
            json response_json = json::parse(response_string);
            if (response_json.contains("output") && response_json["output"].contains("embeddings")) {
                // 🌟 核心提取：把阿里返回的那 1536 个浮点数，一个个装进我们的 C++ vector 里！
                for (auto& val : response_json["output"]["embeddings"][0]["embedding"]) {
                    embedding.push_back(val.get<float>());
                }
            } else {
                std::cerr << "🔴 向量化 API 报错: " << response_string << std::endl;
            }
        } catch (...) {
            std::cerr << "🔴 向量化解析失败: " << response_string << std::endl;
        }
    }
    return embedding;
}

// 🌟 定义一个上下文结构体，用来把回调函数传给 curl
struct StreamContext {
    std::function<void(const std::string&)> callback;
};

// 🌟 核心算法：实时解析阿里云发来的 JSON 碎片
size_t LLMEngine::StreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string chunk((char*)contents, total_size);
    StreamContext* ctx = (StreamContext*)userp;

    size_t pos = 0;
    // 阿里云的数据块都是以 "data: " 开头的
    while ((pos = chunk.find("data: ", pos)) != std::string::npos) {
        pos += 6; // 跳过 "data: "
        size_t end_pos = chunk.find("\n", pos);
        if (end_pos == std::string::npos) end_pos = chunk.size();

        std::string json_str = chunk.substr(pos, end_pos - pos);
        if (json_str.find("[DONE]") != std::string::npos) break; // 结束标志

        try {
            json j = json::parse(json_str);
            if (j.contains("choices") && !j["choices"].empty()) {
                auto& delta = j["choices"][0]["delta"];
                if (delta.contains("content")) {
                    std::string word = delta["content"].get<std::string>();
                    // 🌟 抠出一个词，立刻通过回调传给 ChatServer！
                    if (ctx->callback) ctx->callback(word);
                }
            }
        } catch (...) { /* 忽略被截断的半个 JSON 包 */ }
        pos = end_pos;
    }
    return total_size;
}

// 🌟 发起流式请求
void LLMEngine::CallQwenStream(const std::string& user_message, 
                               const std::string& api_key, 
                               std::function<void(const std::string&)> stream_callback) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    StreamContext ctx { stream_callback };
    std::string api_url = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";

    json request_body = {
        {"model", "qwen-plus"},
        {"stream", true}, // 🌟 关键：命令大模型开启打字机模式！
        {"messages", {
            {{"role", "system"}, {"content", "你是一个友好的群聊 AI 助手。"}},
            {{"role", "user"}, {"content", user_message}}
        }}
    };
    std::string body_str = request_body.dump();

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");

    curl_easy_perform(curl); // 阻塞直到所有流数据接收完毕
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

// 🌟 终极进化：带记忆的流式请求
void LLMEngine::CallQwenStreamWithMemory(
        const std::vector<nlohmann::json>& history, 
        const std::string& current_prompt,
        const std::string& api_key, 
        std::function<void(const std::string&)> stream_callback) {
            
    CURL* curl = curl_easy_init();
    if (!curl) return;

    StreamContext ctx { stream_callback };
    std::string api_url = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";

    // 🌟 核心组装：创建一个动态的 messages 数组
    json messages_array = json::array();
    
    // 1. 塞入系统人设 (System Prompt)
    messages_array.push_back({{"role", "system"}, {"content", "你是一个极其聪明的专属 AI 助手。请记住用户的名字和身份信息。"}});

    // 2. 遍历数据库传来的历史记录，按顺序塞进大模型脑子里！
    for (const auto& msg : history) {
        if (msg["type"] == "user") {
            messages_array.push_back({{"role", "user"}, {"content", msg["text"]}});
        } else if (msg["type"] == "ai") {
            messages_array.push_back({{"role", "assistant"}, {"content", msg["reply"]}});
        }
    }

    // 3. 塞入当前最新的一句话
    messages_array.push_back({{"role", "user"}, {"content", current_prompt}});

    // 把组装好的上下文塞给大模型
    json request_body = {
        {"model", "qwen-plus"},
        {"stream", true},
        {"messages", messages_array}
    };
    std::string body_str = request_body.dump();

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");

    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}