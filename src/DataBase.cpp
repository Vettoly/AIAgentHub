#include "Database.h"
#include <mysqlx/xdevapi.h>
#include <mutex>
#include <iostream>

static std::unique_ptr<mysqlx::Session> g_db;
static std::mutex g_db_mutex;

bool Database::Connect(const std::string& host, int port, const std::string& user, const std::string& password, const std::string& db_name) {
    try {
        g_db = std::make_unique<mysqlx::Session>(host, port, user, password, db_name);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ 数据库连接失败: " << e.what() << std::endl;
        return false;
    }
}

// 🌟 新增：注册逻辑
bool Database::RegisterUser(const std::string& username, const std::string& password) {
    if (!g_db) return false;
    std::lock_guard<std::mutex> lock(g_db_mutex);
    try {
        // 1. 查重：看看用户名是不是被占用了
        auto res = g_db->sql("SELECT id FROM Users WHERE username = ?").bind(username).execute();
        if (res.count() > 0) return false; 
        
        // 2. 插入新用户
        g_db->sql("INSERT INTO Users (username, password) VALUES (?, ?)").bind(username, password).execute();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ 注册失败: " << e.what() << std::endl;
        return false;
    }
}

// 🌟 新增：登录校验逻辑
bool Database::VerifyUser(const std::string& username, const std::string& password) {
    if (!g_db) return false;
    std::lock_guard<std::mutex> lock(g_db_mutex);
    try {
        auto res = g_db->sql("SELECT password FROM Users WHERE username = ?").bind(username).execute();
        if (res.count() == 0) return false; // 没这个用户
        
        std::string db_pwd = res.fetchOne()[0].get<std::string>();
        return db_pwd == password; // 对比密码
    } catch (const std::exception& e) {
        std::cerr << "❌ 登录查库失败: " << e.what() << std::endl;
        return false;
    }
}

// 🌟 升级：保存消息时必须带用户名
void Database::SaveMessage(const std::string& username, const std::string& sender_type, const std::string& content) {
    if (!g_db) return;
    std::lock_guard<std::mutex> lock(g_db_mutex);
    try {
        // 插入记录时，将 username, sender_type (user/ai), message 存入
        g_db->sql("INSERT INTO ChatHistory (username, sender_type, message) VALUES (?, ?, ?)")
            .bind(username, sender_type, content).execute();
    } catch (const std::exception& e) {
        std::cerr << "💾 数据库写入失败: " << e.what() << std::endl;
    }
}

// 🌟 升级：用 WHERE 语句筛选属于该用户的记录
std::vector<nlohmann::json> Database::LoadHistory(const std::string& username) {
    std::vector<nlohmann::json> history;
    if (!g_db) return history;
    
    std::lock_guard<std::mutex> lock(g_db_mutex);
    try {
        auto res = g_db->sql("SELECT sender_type, message FROM (SELECT sender_type, message, created_at FROM ChatHistory WHERE username = ? ORDER BY created_at DESC LIMIT 20) sub ORDER BY created_at ASC")
            .bind(username).execute();
            
        for (auto row : res) {
            std::string type = row[0].get<std::string>();
            std::string content = row[1].get<std::string>();

            nlohmann::json h_msg;
            if (type == "user") {
                h_msg = {{"type", "user"}, {"sender", username}, {"text", content}};
            } else if (type == "ai") {
                h_msg = {{"type", "ai"}, {"target", username}, {"reply", content}};
            }
            history.push_back(h_msg);
        }
    } catch (const std::exception& e) {
        std::cerr << "💾 读取历史记录失败: " << e.what() << std::endl;
    }
    return history;
}

// ================== 🌟 权限与业务层 ==================

std::string Database::GetUserRole(const std::string& username) {
    if (!g_db) return "user"; // 默认安全降级为普通用户
    std::lock_guard<std::mutex> lock(g_db_mutex);
    try {
        auto res = g_db->sql("SELECT role FROM Users WHERE username = ?").bind(username).execute();
        if (res.count() > 0) {
            return res.fetchOne()[0].get<std::string>();
        }
    } catch (...) {}
    return "user";
}

bool Database::AddScore(const std::string& student_name, const std::string& subject, int score) {
    if (!g_db) return false;
    std::lock_guard<std::mutex> lock(g_db_mutex);
    try {
        // REPLACE INTO：如果有相同名字和科目的记录，就直接覆盖更新分数
        g_db->sql("REPLACE INTO Scores (student_name, subject, score) VALUES (?, ?, ?)")
            .bind(student_name, subject, score).execute();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ 成绩录入失败: " << e.what() << std::endl;
        return false;
    }
}

// 🌟 AI 工具函数：查成绩
int Database::GetScore(const std::string& student_name, const std::string& subject) {
    if (!g_db) return -1;
    std::lock_guard<std::mutex> lock(g_db_mutex);
    try {
        auto res = g_db->sql("SELECT score FROM Scores WHERE student_name = ? AND subject = ?")
            .bind(student_name, subject).execute();
        if (res.count() > 0) {
            return res.fetchOne()[0].get<int>(); // 查到了，返回具体分数
        }
    } catch (...) {}
    return -1; // -1 表示没查到
}