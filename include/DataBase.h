#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class Database {
public:
    // 连接数据库
    static bool Connect(const std::string& host, int port, const std::string& user, const std::string& password, const std::string& db_name);
    
    // 🌟 注册接口 (用户名存在则返回 false)
    static bool RegisterUser(const std::string& username, const std::string& password);
    
    // 🌟 登录接口 (密码正确返回 true)
    static bool VerifyUser(const std::string& username, const std::string& password);

    // 🌟 保存聊天记录时，必须带上这是哪个 username 的对话
    static void SaveMessage(const std::string& username, const std::string& sender_type, const std::string& content);
    
    // 🌟 只读取属于特定 username 的历史记录
    static std::vector<nlohmann::json> LoadHistory(const std::string& username);

    // 🌟  获取用户角色 (是 admin 还是 user)
    static std::string GetUserRole(const std::string& username);
    
    // 🌟  管理员专属，向数据库录入成绩
    static bool AddScore(const std::string& student_name, const std::string& subject, int score);

    // 🌟  供 AI 调用的查询成绩工具
    static int GetScore(const std::string& student_name, const std::string& subject);
};