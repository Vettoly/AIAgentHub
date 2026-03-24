#pragma once
#include <string>
#include <vector>

// 🌟定义一个“知识块”，把文字和它的 1536 维向量死死绑在一起
struct KnowledgeChunk {
    std::string text;
    std::vector<float> embedding;
};

class RAGEngine {
public:
    // 1. 系统启动时调用：读取文件，全量向量化，存入大脑
    static void InitKnowledgeBase(const std::string& file_path, const std::string& api_key);
    
    // 2. 聊天时调用：把用户的问题变成向量，算出夹角，找出最匹配的那段话
    static std::string RetrieveMostRelevant(const std::string& user_query, const std::string& api_key);

private:
    // 🧮 核心算法：用 C++ 极速计算两个高维向量的余弦相似度
    static float CosineSimilarity(const std::vector<float>& vecA, const std::vector<float>& vecB);
    
    // 我们私有知识库的全局存储容器
    static std::vector<KnowledgeChunk> knowledge_base_;
};