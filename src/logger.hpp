#pragma once
// tangbase::Logger — 线程安全的日志输出工具，支持控制台、文件以及按时间轮转三种模式
// 日志级别：debug / info / warn / error
// 依赖: tangbase::format (util/format.hpp)

#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <chrono>
#include <filesystem>
#include <vector>
#include <algorithm>

#include "util/format.hpp"

namespace tangbase {

// 日志级别枚举，按严重程度递增
enum class LogLevel { debug, info, warn, error };

// Logger — 线程安全的日志记录器，单例模式
// 默认输出到标准输出，可通过 outfile() 切换到文件，或 set_rotation() 启用时间轮转
class Logger {
public:
    // 日志轮转配置选项
    struct RotationOptions {
        int64_t rotation_interval_ms = 86400000;  // 默认每天轮转
        size_t max_files = 10;                     // 默认保留 10 个文件
    };

    // 获取单例实例
    static Logger& instance() {
        static Logger log;
        return log;
    }

    // 禁止拷贝
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 设置输出文件（追加模式），会关闭轮转模式
    static bool outfile(std::string_view filename) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (filename.empty()) return false;
        std::ofstream new_ofs(std::string(filename), std::ios::app);
        if (!new_ofs) return false;
        ofs_.close();
        ofs_ = std::move(new_ofs);
        rotation_enabled_ = false;
        target_ = Target::file;
        return true;
    }

    // 切换到命令行输出
    static void outconsole() {
        std::unique_lock<std::mutex> lock(mutex_);
        ofs_.close();
        rotation_enabled_ = false;
        target_ = Target::cout;
    }

    // 启用日志轮转（path_pattern 支持 strftime 格式化，如 "logs/app_{}%Y-%m-%d{}.log"）
    // 返回 true 表示成功启用，false 表示路径无效或无法创建文件
    static bool set_rotation(std::string_view path_pattern, RotationOptions opts = {}) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (path_pattern.empty()) return false;
        rotation_path_pattern_ = std::string(path_pattern);
        rotation_options_ = std::move(opts);
        last_rotation_time_ = std::chrono::system_clock::now();
        ofs_.close();
        target_ = Target::file;
        rotation_enabled_ = true;
        open_rotated_file_(std::chrono::system_clock::now());
        if (!rotation_enabled_) {
            // open_rotated_file_ 内部回退，说明打开失败
            return false;
        }
        return true;
    }

    // 强制立即轮转
    static void rotate() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!rotation_enabled_) return;
        do_rotate_(std::chrono::system_clock::now());
    }

    // 记录日志
    // level — 日志级别
    // fmt   — 格式化字符串
    // args  — 格式化参数
    template<class... Args>
    void log(LogLevel level, std::string_view fmt, const Args&... args) {
        log(level, format(fmt, args...));
    }

    // 直接记录原始字符串消息（不做格式化）
    void log(LogLevel level, std::string_view msg) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (rotation_enabled_) {
            auto now = std::chrono::system_clock::now();
            if (now - last_rotation_time_ >= std::chrono::milliseconds(rotation_options_.rotation_interval_ms)) {
                do_rotate_(now);
            }
        }

        thread_local std::string buf;
        buf.clear();
        buf.reserve(4096);
        auto ts = now_iso();
        auto lv = level_str(level);
        buf.append(ts).append(" [").append(lv).append("] ").append(msg).push_back('\n');

        if (target_ == Target::file && ofs_) {
            ofs_ << buf;
            ofs_.flush();
        } else {
            std::cout << buf;
        }
    }

private:
    enum class Target { cout, file };

    Logger() = default;

    static std::string_view level_str(LogLevel level) {
        switch (level) {
            case LogLevel::debug: return "DEBUG";
            case LogLevel::info:  return "INFO ";
            case LogLevel::warn:  return "WARN ";
            case LogLevel::error: return "ERROR";
        }
        return "?????";
    }

    static std::string now_iso() {
        auto now = std::time(nullptr);
        char buf[32] = {};
        std::tm tm = {};
#if defined(_WIN32)
        localtime_s(&tm, &now);
#else
        localtime_r(&now, &tm);
#endif
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        return buf;
    }

    // 生成轮转后的文件名（根据 path_pattern 和当前时间生成）
    // 模式中的 {} 会被替换为空字符串后再传给 strftime
    static std::string make_rotated_path_(const std::string& pattern, std::chrono::system_clock::time_point now) {
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        char buf[128] = {};
        std::tm tm = {};
#if defined(_WIN32)
        localtime_s(&tm, &now_time_t);
#else
        localtime_r(&now_time_t, &tm);
#endif
        // Strip {} placeholders before calling strftime
        std::string stripped;
        stripped.reserve(pattern.size());
        for (size_t i = 0; i < pattern.size(); ++i) {
            if (i < pattern.size() - 1 && pattern[i] == '{' && pattern[i + 1] == '}') {
                ++i; // skip both chars
            } else {
                stripped.push_back(pattern[i]);
            }
        }
        std::strftime(buf, sizeof(buf), stripped.c_str(), &tm);
        return buf;
    }

    // 打开新的轮转文件，失败时回退到控制台并禁用轮转
    // 若目标路径与 current_log_path_ 相同则跳过（同日追加）
    static void open_rotated_file_(std::chrono::system_clock::time_point now) {
        auto path = make_rotated_path_(rotation_path_pattern_, now);

        // 路径未变，继续追加，无需重新打开
        if (path == current_log_path_ && ofs_) return;

        ofs_.close();
        current_log_path_ = path;
        try {
            std::filesystem::create_directories(std::filesystem::path(path).parent_path());
            ofs_.open(path, std::ios::app);
            if (!ofs_) throw std::runtime_error("cannot open file");
        } catch (...) {
            // 打开失败，回退到控制台并禁用轮转
            rotation_enabled_ = false;
            target_ = Target::cout;
            std::cerr << "Logger: failed to open rotated file, fallback to console\n";
        }
    }

    // 执行轮转：检查是否需要切换文件，必要时打开新文件并清理旧文件
    static void do_rotate_(std::chrono::system_clock::time_point now) {
        auto new_path = make_rotated_path_(rotation_path_pattern_, now);
        // 路径未变（同一天），继续追加，不重新打开
        if (new_path == current_log_path_ && ofs_) {
            last_rotation_time_ = now;
            return;
        }
        ofs_.close();
        open_rotated_file_(now);
        last_rotation_time_ = now;
        purge_rotated_files_();
    }

    // 删除超出数量限制的旧日志文件（按最后修改时间排序，清理目录下所有普通文件）
    static void purge_rotated_files_() {
        auto parent = std::filesystem::path(rotation_path_pattern_).parent_path();
        if (parent.empty()) parent = ".";
        if (!std::filesystem::exists(parent)) return;

        std::vector<std::filesystem::path> files;
        try {
            for (auto& entry : std::filesystem::directory_iterator(parent)) {
                if (entry.is_regular_file())
                    files.push_back(entry.path());
            }
        } catch (...) {
            return;
        }

        if (files.size() <= rotation_options_.max_files) return;

        // 按修改时间排序（oldest first）
        std::sort(files.begin(), files.end(),
            [](const auto& a, const auto& b) {
                return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
            });

        // 删除最旧的文件，直到不超过 max_files
        for (size_t i = 0; i + rotation_options_.max_files < files.size(); ++i) {
            try {
                std::filesystem::remove(files[i]);
            } catch (...) {
                // ignore removal errors
            }
        }
    }

    static Target target_;
    static std::ofstream ofs_;
    static std::mutex mutex_;

    // 轮转相关状态
    static bool rotation_enabled_;
    static std::string rotation_path_pattern_;
    static RotationOptions rotation_options_;
    static std::chrono::system_clock::time_point last_rotation_time_;
    static std::string current_log_path_;
};

// 静态成员定义
inline Logger::Target Logger::target_ = Logger::Target::cout;
inline std::ofstream Logger::ofs_;
inline std::mutex Logger::mutex_;

// 轮转相关状态初始化
inline bool Logger::rotation_enabled_ = false;
inline std::string Logger::rotation_path_pattern_;
inline Logger::RotationOptions Logger::rotation_options_;
inline std::chrono::system_clock::time_point Logger::last_rotation_time_{};
inline std::string Logger::current_log_path_;

// 全局便捷函数：使用单例 Logger 输出日志
template<class... Args>
void log_debug(std::string_view fmt, const Args&... args) {
    Logger::instance().log(LogLevel::debug, fmt, args...);
}
template<class... Args>
void log_info(std::string_view fmt, const Args&... args) {
    Logger::instance().log(LogLevel::info, fmt, args...);
}
template<class... Args>
void log_warn(std::string_view fmt, const Args&... args) {
    Logger::instance().log(LogLevel::warn, fmt, args...);
}
template<class... Args>
void log_error(std::string_view fmt, const Args&... args) {
    Logger::instance().log(LogLevel::error, fmt, args...);
}

}  // namespace tangbase