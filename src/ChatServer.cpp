#include "ChatServer.h"
#include "LLMEngine.h"
#include "Database.h"
#include "RAGEngine.h"

#include <iostream>
#include <string>
#include <memory>
#include <utility>
#include <thread>
#include <vector>
#include <set>
#include <mutex>
#include <sstream> 

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

class WebSocketSession; 
static std::set<WebSocketSession*> active_sessions; 
static std::mutex session_mutex;                    

static void BroadcastToAll(const std::string& msg);

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::vector<std::string> write_queue_;
    bool is_writing_ = false;
    
    // 会话状态变量
    bool is_logged_in_ = false;
    std::string my_name_; 

public:
    explicit WebSocketSession(tcp::socket&& socket) : ws_(std::move(socket)) {}

    ~WebSocketSession() {
        std::lock_guard<std::mutex> lock(session_mutex);
        active_sessions.erase(this);
    }

    void start() {
        {
            std::lock_guard<std::mutex> lock(session_mutex);
            active_sessions.insert(this);
        }
        ws_.async_accept(beast::bind_front_handler(&WebSocketSession::on_accept, shared_from_this()));
    }

    void send_message(std::string msg) {
        net::post(ws_.get_executor(), [this, self = shared_from_this(), msg = std::move(msg)]() {
            write_queue_.push_back(msg); 
            if (!is_writing_) do_write();
        });
    }

private:
    void do_write() {
        is_writing_ = true;
        ws_.async_write(net::buffer(write_queue_.front()), 
            [this, self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (ec) return; 
                write_queue_.erase(write_queue_.begin()); 
                if (!write_queue_.empty()) do_write(); 
                else is_writing_ = false; 
            });
    }

    void on_accept(beast::error_code ec) {
        if (ec) return;
        this->send_message(json{{"type", "system"}, {"text", "🔗 已连接服务器，等待鉴权..."}}.dump());
        do_read();
    }

    void do_read() {
        ws_.async_read(buffer_, beast::bind_front_handler(&WebSocketSession::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec == websocket::error::closed) {
            if (is_logged_in_) {
                json leave_msg = {{"type", "system"}, {"text", "👋 " + my_name_ + " 退出了系统"}};
                BroadcastToAll(leave_msg.dump());
            }
            return;
        }
        if (ec) return;

        std::string raw_msg = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size()); 

        try {
            json client_msg = json::parse(raw_msg);
            std::string msg_type = client_msg.value("type", "");

            // ================== 【拦截器：未登录状态】 ==================
            if (!is_logged_in_) {
                if (msg_type == "login") {
                    std::string user = client_msg.value("username", "");
                    std::string pwd = client_msg.value("password", "");
                    
                    if (Database::VerifyUser(user, pwd)) {
                        is_logged_in_ = true;
                        my_name_ = user;
                        this->send_message(json{{"type", "login_success"}, {"username", user}}.dump());
                        
                        auto history = Database::LoadHistory(my_name_);
                        for(const auto& h : history) {
                            this->send_message(h.dump());
                        }

                        json join_msg = {{"type", "system"}, {"text", "🎉 [" + my_name_ + "] 登录了系统"}};
                        BroadcastToAll(join_msg.dump());
                    } else {
                        this->send_message(json{{"type", "system"}, {"text", "❌ 登录失败：用户名或密码错误"}}.dump());
                    }
                } 
                else if (msg_type == "register") {
                    std::string user = client_msg.value("username", "");
                    std::string pwd = client_msg.value("password", "");
                    if (Database::RegisterUser(user, pwd)) {
                        this->send_message(json{{"type", "system"}, {"text", "✅ 注册成功！请点击登录"}}.dump());
                    } else {
                        this->send_message(json{{"type", "system"}, {"text", "❌ 注册失败：用户名可能已被占用"}}.dump());
                    }
                } 
                else {
                    this->send_message(json{{"type", "system"}, {"text", "⚠️ 非法请求，请先登录！"}}.dump());
                }
                do_read();
                return;
            }

            // ================== 【通行区：已登录状态处理聊天】 ==================
            if (msg_type == "chat") {
                std::string chat_text = client_msg.value("text", "");

                // 🌟 1. 超级管理员后门指令拦截
                if (chat_text.rfind("/addscore ", 0) == 0) {
                    if (Database::GetUserRole(my_name_) != "admin") {
                        this->send_message(json{{"type", "system"}, {"text", "❌ 权限不足：警告，你不是系统管理员！"}}.dump());
                        return; // 拦截！不往下走
                    }

                    std::istringstream iss(chat_text);
                    std::string cmd, stu_name, subject;
                    int score;
                    if (iss >> cmd >> stu_name >> subject >> score) {
                        if (Database::AddScore(stu_name, subject, score)) {
                            this->send_message(json{{"type", "system"}, {"text", "✅ 绝密操作：成功为 [" + stu_name + "] 录入 [" + subject + "] 成绩：" + std::to_string(score)}}.dump());
                        } else {
                            this->send_message(json{{"type", "system"}, {"text", "❌ 录入失败：数据库写入异常"}}.dump());
                        }
                    } else {
                        this->send_message(json{{"type", "system"}, {"text", "⚠️ 格式错误！标准语法：/addscore 姓名 科目 分数"}}.dump());
                    }
                    return; // 🌟 关键：拦截完毕，结束处理
                }

                // 🌟 2. 普通聊天逻辑
                Database::SaveMessage(my_name_, "user", chat_text);

                json user_msg = {{"type", "user"}, {"sender", my_name_}, {"text", chat_text}};
                BroadcastToAll(user_msg.dump());

                json thinking_msg = {{"type", "system"}, {"text", "🤖 AI 正在为 " + my_name_ + " 思考..."}};
                BroadcastToAll(thinking_msg.dump());

                // 开启独立线程呼叫大模型
                std::thread([chat_text, my_name = this->my_name_]() {
                    std::string api_key = "你的百炼API_KEY"; 
                    
                    auto history = Database::LoadHistory(my_name);
                    
                    // ================= 🌟 终极核心：AI Agent 工具调用 =================
                    std::string tool_injection = "";
                    
                    // 1. 意图嗅探：用户是不是想查成绩？
                    if (chat_text.find("成绩") != std::string::npos || chat_text.find("分") != std::string::npos) {
                        // 2. 槽位提取 (Slot Filling)：让 LLM 帮我们抠出参数
                        std::string extract_prompt = "请提取以下句子中用户想查询的姓名和科目，严格只输出JSON格式（如：{\"name\":\"张三\", \"subject\":\"物理\"}）。如果找不到对应信息，只输出 {}。用户句子：" + chat_text;
                        
                        // 发起一个极速的非流式请求
                        std::string args_json_str = LLMEngine::CallQwen(extract_prompt, api_key);
                        
                        try {
                            json args = json::parse(args_json_str);
                            if (args.contains("name") && args.contains("subject")) {
                                std::string n = args["name"].get<std::string>();
                                std::string s = args["subject"].get<std::string>();
                                
                                // 3. C++ 亲自下场执行本地函数查库！
                                int score = Database::GetScore(n, s);
                                if (score >= 0) {
                                    tool_injection = "\n【系统后台查询结果：数据库显示 " + n + " 的 " + s + " 成绩是 " + std::to_string(score) + " 分】\n";
                                } else {
                                    tool_injection = "\n【系统后台查询结果：数据库中未找到 " + n + " 的 " + s + " 成绩记录】\n";
                                }
                            }
                        } catch (...) { /* JSON解析失败说明AI没提取出来，忽略即可 */ }
                    }
                    // ==================================================================

                    // 常规 RAG 知识检索
                    std::string context = RAGEngine::RetrieveMostRelevant(chat_text, api_key);
                    
                    // 终极 Prompt 融合组装！
                    std::string final_prompt = chat_text;
                    if (!context.empty() || !tool_injection.empty()) {
                        final_prompt = "参考资料：" + context + tool_injection + "\n请根据参考资料回答用户问题：" + chat_text;
                    }

                    std::string full_reply = "";

                    // 启动打字机，流式输出！
                    LLMEngine::CallQwenStreamWithMemory(history, final_prompt, api_key, [&](const std::string& word) {
                        full_reply += word;
                        json stream_msg = {
                            {"type", "ai_stream"}, 
                            {"text", word},
                            {"target", my_name}
                        };
                        BroadcastToAll(stream_msg.dump());
                    });

                    json end_msg = {{"type", "ai_stream_end"}, {"target", my_name}};
                    BroadcastToAll(end_msg.dump());
                    
                    Database::SaveMessage(my_name, "ai", full_reply);
                }).detach();
            }

        } catch (const std::exception& e) {
            std::cerr << "⚠️ 无法解析客户端消息: " << e.what() << std::endl;
        }

        do_read(); 
    }
};

static void BroadcastToAll(const std::string& msg) {
    std::lock_guard<std::mutex> lock(session_mutex);
    for (auto* session : active_sessions) {
        session->send_message(msg);
    }
}

class WebSocketServerImpl {
    tcp::acceptor acceptor_;
public:
    WebSocketServerImpl(net::io_context& ioc, short port)
        : acceptor_(ioc, tcp::endpoint(tcp::v4(), port)) {
        std::cout << "🚀 AI Agent Hub [带权限控制版] 已启动！端口: " << port << std::endl;
        do_accept();
    }
private:
    void do_accept() {
        acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) std::make_shared<WebSocketSession>(std::move(socket))->start();
            do_accept(); 
        });
    }
};

void ChatServer::Run(short port) {
    try {
        net::io_context ioc;
        WebSocketServerImpl server(ioc, port);
        ioc.run();
    } catch (std::exception& e) { 
        std::cerr << "❌ 网络层致命错误: " << e.what() << std::endl; 
    }
}
