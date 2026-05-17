#pragma once
// tangbase::util — 通用工具函数命名空间
// 所有工具函数均为纯函数，无状态，无副作用（除非另有说明）

#include <string>
#include <string_view>
#include <vector>

namespace tangbase::util {

// 去除字符串两端的空白字符（空格、\t、\n、\r）
// 返回一个新的 std::string，不修改原字符串视图
inline std::string trim(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' || s[start] == '\r')) ++start;
    size_t end = s.size();
    while (end > start && (s[end-1] == ' ' || s[end-1] == '\t' || s[end-1] == '\n' || s[end-1] == '\r')) --end;
    return std::string(s.substr(start, end - start));
}

// 去除字符串左端（开头）的空白字符，返回视图
inline std::string_view trim_start(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' || s[start] == '\r')) ++start;
    return s.substr(start);
}

// 去除字符串右端（结尾）的空白字符，返回视图
inline std::string_view trim_end(std::string_view s) {
    size_t end = s.size();
    while (end > 0 && (s[end-1] == ' ' || s[end-1] == '\t' || s[end-1] == '\n' || s[end-1] == '\r')) --end;
    return s.substr(0, end);
}

// 按单个分隔符拆分字符串
// 例如: split("a,b,c", ',') -> ["a", "b", "c"]
// 不忽略空字符串（"a,,b" -> ["a", "", "b"]）
inline std::vector<std::string> split(std::string_view s, char delim) {
    std::vector<std::string> result;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == delim) {
            result.emplace_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return result;
}

// 按多个分隔符拆分字符串，跳过连续的多个分隔符
// 例如: split_any("a,b;c", ",;") -> ["a", "b", "c"]
inline std::vector<std::string> split_any(std::string_view s, std::string_view delims) {
    std::vector<std::string> result;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        bool is_delim = (i < s.size()) && (delims.find(s[i]) != std::string_view::npos);
        bool at_end = (i == s.size());
        if (is_delim || at_end) {
            if (start < i) {
                result.emplace_back(s.substr(start, i - start));
            }
            start = i + 1;
        }
    }
    return result;
}

// 将第一个匹配的 old 替换为 new（只替换一次）
// old 不能为空，否则返回原字符串副本
inline std::string replace_once(std::string_view s, std::string_view old, std::string_view new_) {
    if (old.empty()) return std::string(s);
    size_t pos = s.find(old);
    if (pos == std::string_view::npos) return std::string(s);
    std::string r;
    r.append(s.substr(0, pos));
    r.append(new_);
    r.append(s.substr(pos + old.size()));
    return r;
}

// 将所有匹配的 old 替换为 new（old 不能为空）
// 替换是非重叠的，从左到右依次进行
inline std::string replace_all(std::string_view s, std::string_view old, std::string_view new_) {
    if (old.empty()) return std::string(s);
    std::string r;
    size_t start = 0;
    while (start < s.size()) {
        size_t pos = s.find(old, start);
        if (pos == std::string_view::npos) {
            r.append(s.substr(start));
            break;
        }
        r.append(s.substr(start, pos - start));
        r.append(new_);
        start = pos + old.size();
    }
    return r;
}

// 将字符串转为小写（仅处理 ASCII 字母）
inline std::string to_lower(std::string_view s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) r.push_back((c >= 'A' && c <= 'Z') ? (c + 32) : c);
    return r;
}

// 将字符串转为大写（仅处理 ASCII 字母）
inline std::string to_upper(std::string_view s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) r.push_back((c >= 'a' && c <= 'z') ? (c - 32) : c);
    return r;
}

// 判断字符串是否以 prefix 开头
inline bool starts_with(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

// 判断字符串是否以 suffix 结尾
inline bool ends_with(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

// 判断字符串是否全部为空白（不含任何非空白字符）
inline bool is_blank(std::string_view s) {
    for (char c : s) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') return false;
    }
    return true;
}

}  // namespace tangbase::util