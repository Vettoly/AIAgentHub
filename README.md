# 🌐 AI Agent Hub

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![CMake](https://img.shields.io/badge/CMake-Build-success.svg)
![MySQL](https://img.shields.io/badge/MySQL-Database-orange.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

AI Agent Hub 是一个基于 **纯 C++** 构建的企业级智能体（Agent）后端架构。

本项目摒弃了传统的 Python 包装，直接在 C++ 底层实现了 **大模型流式对话**、**RAG（检索增强生成）** 以及核心的 **Function Calling（工具调用）** 能力。结合自定义的 Reactor 网络模型与完整的 RBAC 鉴权体系，打造了一个高性能、带状态（长时记忆）、可直接操作底层业务数据的 AI 服务中枢。

## ✨ 核心特性

* 🚀 **高并发全双工通信**：基于 `Boost.Asio` 封装 WebSocket 服务器，原生支持多线程处理与读写锁分离，轻松应对高频对话流。
* 🧠 **纯 C++ RAG 引擎**：无需依赖庞大的 Python 生态，原生集成 Embedding API，利用底层 C++ 手写高维向量余弦相似度（Cosine Similarity）算法，实现极速私有知识库召回。
* 🤖 **Agentic 架构与工具调用**：实现基于“意图嗅探”的 Slot Filling（槽位提取）机制。AI 可根据对话上下文，跨界调用底层的 C++ 函数（如：自动穿透查询 MySQL 成绩记录），真正实现从“聊天机器人”到“智能体”的跨越。
* ⚡ **SSE 极速流式解析**：深度定制 `libcurl` 回调拦截器，在 C++ 层面对 Server-Sent Events (SSE) 协议进行碎片级的 JSON 粘包处理，实现丝滑的“打字机”输出体验。
* 🔐 **RBAC 鉴权与海马体记忆**：内置 MySQL 连接池与状态机拦截器。支持用户注册/登录，独立管理个人长时记忆上下文。设立超级管理员权限隔离，保障底层数据落库安全。

## 🛠️ 技术栈

* **核心语言**：C++ 17
* **网络与异步**：Boost.Asio, Boost.Beast
* **数据库**：MySQL (mysqlx/xdevapi)
* **网络请求**：libcurl
* **数据序列化**：nlohmann/json
* **大模型引擎**：阿里云百炼 (Qwen-Plus, Text-Embedding-v1)
* **前端界面**：原生 HTML5 + CSS3 + JS (极客风悬浮 UI)

## 📂 项目架构

```text
AIAgentHub/
├── include/           # 头文件目录
│   ├── ChatServer.h   # WebSocket 网络与状态机
│   ├── Database.h     # 数据库连接池与 CRUD
│   ├── LLMEngine.h    # 大模型请求、流式解析与上下文组装
│   └── RAGEngine.h    # 文本切片与向量检索
├── src/               # 核心实现代码
│   ├── main.cpp       # 引擎组装与启动入口
│   ├── ...
├── docs/
│   └── knowledge.txt  # RAG 本地私有知识库
├── frontend/
│   └── index.html     # 带鉴权的聊天交互大厅
└── CMakeLists.txt     # CMake 构建配置

🚀 快速启动
1. 环境准备
确保已安装 CMake (>= 3.15) 和支持 C++17 的编译器 (MSVC/GCC)。

安装依赖库：boost, curl, nlohmann-json, mysql-connector-cpp (推荐使用 vcpkg 管理)。

准备一个 MySQL 服务器 (>= 8.0)。

2. 数据库初始化
在 MySQL 中执行以下 SQL 脚本，完成底层鉴权表与业务表的构建：
CREATE DATABASE IF NOT EXISTS AIAgentHubDB;
USE AIAgentHubDB;

-- 创建用户表及超级管理员账号
CREATE TABLE IF NOT EXISTS Users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password VARCHAR(255) NOT NULL,
    role VARCHAR(20) DEFAULT 'user' NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
INSERT IGNORE INTO Users (username, password, role) VALUES ('admin', 'admin123', 'admin');

-- 创建历史记录漫游表
CREATE TABLE ChatHistory (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL,
    sender_type VARCHAR(20) NOT NULL, 
    message TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 创建业务测试表（成绩单）
CREATE TABLE IF NOT EXISTS Scores (
    id INT AUTO_INCREMENT PRIMARY KEY,
    student_name VARCHAR(50) NOT NULL,
    subject VARCHAR(50) NOT NULL,
    score INT NOT NULL,
    UNIQUE KEY unique_student_subject (student_name, subject) 
);

3. 配置密钥
在编译前，请打开 src/main.cpp 和 src/ChatServer.cpp，将占位符替换为你真实的配置：

你的阿里云百炼 API Key。

你的 MySQL 数据库密码。

4. 编译与运行
mkdir build && cd build
cmake ..
cmake --build . --config Release
./AIAgentHub
服务启动后，双击打开 frontend/index.html，即可体验智能体大厅。

🎯 绝密指令 (Admin Only)
系统内置了 RBAC 权限隔离。当使用 admin 账号登录时，可在聊天框直接输入特权指令操作底层数据（普通用户会被无情拦截）：
/addscore [姓名] [科目] [分数]
示例：/addscore 张三 C++程序设计 95
录入成绩后，普通用户可直接使用自然语言向 AI 提问（如：“张三的 C++ 考了多少分？”），触发 AI 的 Agent Function Calling 能力进行查询。

欢迎自由探索与构建。