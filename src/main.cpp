#include "Database.h"
#include "LLMEngine.h"
#include "ChatServer.h"
#include "RAGEngine.h"
#include <iostream>
#include <windows.h>

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "========== AI Agent Hub ==========" << std::endl;

    // 1. 初始化 AI 引擎环境
    LLMEngine::Init();

    // 🌟 2. 启动 RAG 引擎，读取本地知识并转换为高维向量！
    // 🔑 填入你的真实 API Key
    std::string my_api_key = "你的百炼API_KEY"; 
    RAGEngine::InitKnowledgeBase("docs/knowledge.txt", my_api_key);

    // 3. 连接数据库 (🔑 请填入你的真实密码)
    std::string my_db_password = "数据库密码"; 
    if (Database::Connect("127.0.0.1", 33060, "root", my_db_password, "AIAgentHubDB")) {
        std::cout << "✅ 数据库连接成功！海马体已激活！" << std::endl;
    } else {
        std::cerr << "❌ 启动失败，请检查数据库状态。" << std::endl;
        return -1;
    }

    // 4. 启动高并发聊天服务器 (阻塞运行)
    ChatServer::Run(8080);

    // 5. 程序结束时清理环境
    LLMEngine::Cleanup();
    return 0;
}
