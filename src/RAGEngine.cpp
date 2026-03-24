#include "RAGEngine.h"
#include "LLMEngine.h"
#include <fstream>
#include <iostream>
#include <cmath> // 引入数学库算平方根

// 初始化静态成员变量
std::vector<KnowledgeChunk> RAGEngine::knowledge_base_;

// 🧮 RAG 核心算法实现：求内积除以各自的模长
float RAGEngine::CosineSimilarity(const std::vector<float>& vecA, const std::vector<float>& vecB) {
    if (vecA.empty() || vecB.empty() || vecA.size() != vecB.size()) return 0.0f;
    float dot_product = 0.0f, normA = 0.0f, normB = 0.0f;
    for (size_t i = 0; i < vecA.size(); ++i) {
        dot_product += vecA[i] * vecB[i];
        normA += vecA[i] * vecA[i];
        normB += vecB[i] * vecB[i];
    }
    if (normA == 0.0f || normB == 0.0f) return 0.0f;
    return dot_product / (std::sqrt(normA) * std::sqrt(normB));
}

void RAGEngine::InitKnowledgeBase(const std::string& file_path, const std::string& api_key) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "❌ 无法打开知识库: " << file_path << std::endl;
        return;
    }

    std::string line, current_chunk = "";
    std::vector<std::string> raw_chunks;

    while (std::getline(file, line)) {
        if (line.empty()) {
            if (!current_chunk.empty()) { raw_chunks.push_back(current_chunk); current_chunk = ""; }
        } else { current_chunk += line + "\n"; }
    }
    if (!current_chunk.empty()) raw_chunks.push_back(current_chunk);

    std::cout << "📚 RAG: 文本切片完毕，正在呼叫大模型进行向量化转换..." << std::endl;

    // 🌟 遍历每一段文字，调用大模型 API 获取坐标，并存入私有大脑
    for (const auto& text : raw_chunks) {
        std::vector<float> emb = LLMEngine::GetEmbedding(text, api_key);
        if (!emb.empty()) {
            knowledge_base_.push_back({text, emb});
        }
    }
    std::cout << "✅ RAG: 向量知识库构建完成！共存入 " << knowledge_base_.size() << " 个高维知识块。" << std::endl;
}

std::string RAGEngine::RetrieveMostRelevant(const std::string& user_query, const std::string& api_key) {
    if (knowledge_base_.empty()) return "";

    // 1. 把用户发来的问题，也变成 1536 维的向量
    std::vector<float> query_emb = LLMEngine::GetEmbedding(user_query, api_key);
    if (query_emb.empty()) return "";

    // 2. 遍历知识库，逐个打分，找出最高分（距离最近）的那一段
    float best_score = -1.0f;
    std::string best_match = "";

    for (const auto& chunk : knowledge_base_) {
        float score = CosineSimilarity(query_emb, chunk.embedding);
        if (score > best_score) {
            best_score = score;
            best_match = chunk.text;
        }
    }

    // 3. 设定门槛：如果相似度大于 0.65（这个阈值可以调），说明确实命中了知识点！
    if (best_score > 0.65f) { 
        std::cout << "🔍 [RAG 触发] 命中知识点! 相似度得分: " << best_score << std::endl;
        return best_match;
    }
    return ""; // 如果没找到相关的，就返回空，让 AI 靠自己的常识回答
}