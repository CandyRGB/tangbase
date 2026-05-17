#pragma once
// tangbase::Config — TOML 配置文件读取工具
// 基于 TomlParser（util/toml.hpp）实现，支持路径访问嵌套值和数组
// 依赖: <optional>, <string>, <string_view>, <unordered_map>
//       tangbase::TomlParser (util/toml.hpp)

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "util/toml.hpp"

namespace tangbase {

// Config — TOML 配置文件读取器
// 支持从文件加载或直接解析字符串
// 通过路径访问值，支持嵌套表格和数组下标
class Config {
public:
    // 从文件路径加载 TOML 配置
    // path — 文件路径（UTF-8 编码）
    // 文件不存在或解析失败时返回空的 Config（所有 get* 返回 nullopt）
    static Config parse_file(std::string_view path);

    // 从字符串解析 TOML 配置
    // content — TOML 格式字符串
    // 解析失败时返回空的 Config
    static Config parse_string(std::string_view content);

    // 获取字符串值
    // path — 路径，如 "server.host" 或 "plugins.names[0]"
    // 返回 nullopt 如果路径不存在或类型不匹配
    std::optional<std::string> get_string(std::string_view path) const;

    // 获取整数值
    std::optional<int64_t> get_int(std::string_view path) const;

    // 获取浮点数值
    std::optional<double> get_double(std::string_view path) const;

    // 获取布尔值
    std::optional<bool> get_bool(std::string_view path) const;

    // 获取字符串，路径不存在时返回默认值
    std::string get_string_or(std::string_view path, std::string_view def) const;

    // 获取整数值，路径不存在时返回默认值
    int64_t get_int_or(std::string_view path, int64_t def) const;

    // 获取浮点数值，路径不存在时返回默认值
    double get_double_or(std::string_view path, double def) const;

    // 获取布尔值，路径不存在时返回默认值
    bool get_bool_or(std::string_view path, bool def) const;

private:
    std::shared_ptr<TomlNode::Table> root_;

    // 在配置树中查找路径，支持嵌套表格和数组下标
    // 路径语法:
    //   "section.key"         — 表格中的键
    //   "array[0]"            — 数组元素
    //   "section.array[0].key" — 嵌套路径
    const TomlNode* lookup(std::string_view path) const;
};

// 内部方法：查找路径对应的节点
inline const TomlNode* Config::lookup(std::string_view path) const {
    if (!root_) return nullptr;

    // 找到第一个 '.' 的位置，分割第一段
    size_t dot = path.find('.');
    std::string_view first = (dot == std::string_view::npos) ? path : path.substr(0, dot);

    auto it = root_->find(std::string(first));
    if (it == root_->end()) return nullptr;

    const TomlNode* cur = &it->second;

    // 处理剩余路径（如果有）
    if (dot != std::string_view::npos) {
        std::string_view rest = path.substr(dot + 1);
        while (!rest.empty()) {
            size_t d = rest.find('.');
            size_t b = rest.find('[');
            bool has_bracket = (b != std::string_view::npos);
            size_t end = rest.size();
            if (d != std::string_view::npos && d < end) end = d;
            if (has_bracket && b < end) end = b;

            std::string_view key = rest.substr(0, end);

            // 处理数组下标: table.array[0]
            if (has_bracket) {
                // 先查表格
                auto* tbl = std::get_if<std::shared_ptr<TomlNode::Table>>(&cur->val);
                if (!tbl) return nullptr;
                auto it2 = (*tbl)->find(std::string(key));
                if (it2 == (*tbl)->end()) return nullptr;
                cur = &it2->second;

                // 再处理数组下标
                size_t close = rest.find(']', b);
                if (close == std::string_view::npos) return nullptr;
                size_t idx = 0;
                for (size_t p = b + 1; p < close; ++p)
                    idx = idx * 10 + (rest[p] - '0');

                auto* arr = std::get_if<std::shared_ptr<TomlNode::Array>>(&cur->val);
                if (!arr || idx >= (*arr)->size()) return nullptr;
                cur = &(*arr)->at(idx);
                rest = rest.substr(close + 1);
                if (!rest.empty() && rest[0] == '.') rest = rest.substr(1);
            } else {
                // 普通表格键
                auto* tbl = std::get_if<std::shared_ptr<TomlNode::Table>>(&cur->val);
                if (!tbl) return nullptr;
                auto it2 = (*tbl)->find(std::string(key));
                if (it2 == (*tbl)->end()) return nullptr;
                cur = &it2->second;
                rest = (d == std::string_view::npos) ? std::string_view() : rest.substr(d + 1);
            }
        }
    }

    return cur;
}

inline std::optional<std::string> Config::get_string(std::string_view path) const {
    auto* n = lookup(path);
    if (!n || !std::holds_alternative<std::string>(n->val)) return std::nullopt;
    return std::get<std::string>(n->val);
}

inline std::optional<int64_t> Config::get_int(std::string_view path) const {
    auto* n = lookup(path);
    if (!n || !std::holds_alternative<int64_t>(n->val)) return std::nullopt;
    return std::get<int64_t>(n->val);
}

inline std::optional<double> Config::get_double(std::string_view path) const {
    auto* n = lookup(path);
    if (!n || !std::holds_alternative<double>(n->val)) return std::nullopt;
    return std::get<double>(n->val);
}

inline std::optional<bool> Config::get_bool(std::string_view path) const {
    auto* n = lookup(path);
    if (!n || !std::holds_alternative<bool>(n->val)) return std::nullopt;
    return std::get<bool>(n->val);
}

inline std::string Config::get_string_or(std::string_view path, std::string_view def) const {
    return get_string(path).value_or(std::string(def));
}

inline int64_t Config::get_int_or(std::string_view path, int64_t def) const {
    return get_int(path).value_or(def);
}

inline double Config::get_double_or(std::string_view path, double def) const {
    return get_double(path).value_or(def);
}

inline bool Config::get_bool_or(std::string_view path, bool def) const {
    return get_bool(path).value_or(def);
}

inline Config Config::parse_file(std::string_view path) {
    Config cfg;
    try {
        FILE* f = std::fopen(std::string(path).c_str(), "rb");
        if (!f) return cfg;

        std::string content;
        char buf[512];
        while (std::fgets(buf, sizeof(buf), f)) content += buf;
        std::fclose(f);

        TomlNode root = TomlParser::parse(content);
        cfg.root_ = std::get<std::shared_ptr<TomlNode::Table>>(root.val);
    } catch (...) {
        // 解析失败或文件读取失败时返回空 Config
    }
    return cfg;
}

inline Config Config::parse_string(std::string_view content) {
    Config cfg;
    try {
        TomlNode root = TomlParser::parse(content);
        cfg.root_ = std::get<std::shared_ptr<TomlNode::Table>>(root.val);
    } catch (...) {
        // 解析失败时返回空 Config
    }
    return cfg;
}

}  // namespace tangbase