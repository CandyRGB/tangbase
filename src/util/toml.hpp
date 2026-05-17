#pragma once
// tangbase::util::TomlParser — TOML 配置文件解析器 (TOML v1.0.0)
// 支持: 基本字符串/多行字符串/字面量字符串（含Unicode转义 \uXXXX \UXXXXXXXX）、
//       整数(十进制/十六进制/八进制/二进制，溢出检测)、浮点数(含inf/nan)、布尔值、
//       表格、表数组、内联表、点分隔键、下划线分隔符（位置校验）
// 不支持: 日期时间 (datetime)
// 依赖: C++17, <cstdint>, <cmath>, <cstdio>, <limits>, <memory>, <stdexcept>,
//       <string>, <string_view>, <unordered_map>, <variant>, <vector>

#include <cstdint>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tangbase {

// TOML 解析错误 — 所有解析异常的统一类型
struct TomlError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// TOML 节点，代表一个解析后的 TOML 值
struct TomlNode {
    enum class Type { String, Int, Float, Bool, Table, Array } type;
    using Table  = std::unordered_map<std::string, TomlNode>;
    using Array  = std::vector<TomlNode>;
    std::variant<
        std::string, int64_t, double, bool,
        std::shared_ptr<Table>,
        std::shared_ptr<Array>
    > val;
};

// 类型别名（向后兼容）
using TomlTable = TomlNode::Table;
using TomlArray = TomlNode::Array;

// TomlParser — TOML 解析器，将 TOML 文本解析为 TomlNode 树
class TomlParser {
public:
    // 解析 TOML 格式字符串，返回根节点（类型为 Table）
    // content — TOML 文本
    // 解析失败时抛出 TomlError
    static TomlNode parse(std::string_view content);

private:
    // 解析上下文，携带行号信息
    struct Ctx {
        const std::string& s;
        size_t& i;
        size_t line;
        Ctx(const std::string& str, size_t& idx) : s(str), i(idx), line(1) {}
    };

    // 跳过空白字符（仅空格、\t，不跳过换行）
    static void skip_ws(const std::string& s, size_t& i);

    // 跳过空白字符和换行（不含注释，用于内联表内部）
    static void skip_ws_nl(const std::string& s, size_t& i, size_t& line);

    // 跳过空白字符、换行和注释（用于数组内部和顶层）
    static void skip_ws_nl_comments(const std::string& s, size_t& i, size_t& line);

    // 解析单个 TOML 值
    // allow_comments — 内联表中禁止注释，数组中允许
    static TomlNode parse_value(Ctx& ctx, bool allow_comments);

    // 解析字符串转义序列（i 指向反斜杠后面的字符）
    static void parse_string_escape(Ctx& ctx, std::string& val);

    // 解析 Unicode 转义 \uXXXX 或 \UXXXXXXXX
    static char32_t parse_unicode_escape(Ctx& ctx, int digits);

    // 将 UTF-32 码点编码为 UTF-8 追加到 val
    static void append_utf8(std::string& val, char32_t cp);

    // 解析简单键（裸键或引号键）
    static std::string parse_simple_key(Ctx& ctx);

    // 去除数字字符串中的下划线
    static std::string strip_underscores(std::string_view num);

    // 校验数字字符串中下划线位置的合法性
    static void validate_underscores(std::string_view num, size_t line);

    // 在父表中查找或创建子表
    static TomlTable* ensure_table(TomlTable* parent, const std::string& key, size_t line);

    // 沿路径导航到父表（支持穿越表数组）
    static TomlTable* navigate_to_parent(
        TomlTable* root, const std::vector<std::string>& keys, bool through_arrays, size_t line);

    // 获取节点为表指针；如果不是表则报错
    static TomlTable* get_as_table(TomlNode& node, const std::string& context, size_t line);

    // 溢出安全乘法：a * b，溢出时返回 false
    static bool safe_mul(int64_t a, int64_t b, int64_t& result);

    // 溢出安全加法：a + b，溢出时返回 false
    static bool safe_add(int64_t a, int64_t b, int64_t& result);

    // 报错辅助
    [[noreturn]] static void error(const std::string& msg, size_t line);
};

// ─── 内联实现 ───────────────────────────────────────────────────────────────

inline void TomlParser::error(const std::string& msg, size_t line) {
    throw TomlError("line " + std::to_string(line) + ": " + msg);
}

inline bool TomlParser::safe_mul(int64_t a, int64_t b, int64_t& result) {
    if (a == 0 || b == 0) { result = 0; return true; }
    if (a > 0) {
        if (b > 0) {
            if (a > std::numeric_limits<int64_t>::max() / b) return false;
        } else {
            if (b < std::numeric_limits<int64_t>::min() / a) return false;
        }
    } else {
        if (b > 0) {
            if (a < std::numeric_limits<int64_t>::min() / b) return false;
        } else {
            if (a < std::numeric_limits<int64_t>::max() / b) return false;  // b<0, max/b
        }
    }
    result = a * b;
    return true;
}

inline bool TomlParser::safe_add(int64_t a, int64_t b, int64_t& result) {
    if (b > 0 && a > std::numeric_limits<int64_t>::max() - b) return false;
    if (b < 0 && a < std::numeric_limits<int64_t>::min() - b) return false;
    result = a + b;
    return true;
}

inline void TomlParser::skip_ws(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
}

inline void TomlParser::skip_ws_nl(const std::string& s, size_t& i, size_t& line) {
    while (i < s.size()) {
        if (s[i] == ' ' || s[i] == '\t') { ++i; continue; }
        if (s[i] == '\n') { ++i; ++line; continue; }
        if (s[i] == '\r') {
            ++i;
            if (i < s.size() && s[i] == '\n') ++i;
            ++line; continue;
        }
        break;
    }
}

inline void TomlParser::skip_ws_nl_comments(const std::string& s, size_t& i, size_t& line) {
    while (i < s.size()) {
        if (s[i] == ' ' || s[i] == '\t') { ++i; continue; }
        if (s[i] == '\n') { ++i; ++line; continue; }
        if (s[i] == '\r') {
            ++i;
            if (i < s.size() && s[i] == '\n') ++i;
            ++line; continue;
        }
        if (s[i] == '#') {
            while (i < s.size() && s[i] != '\n') ++i;
            continue;
        }
        break;
    }
}

inline std::string TomlParser::strip_underscores(std::string_view num) {
    std::string r;
    r.reserve(num.size());
    for (char c : num)
        if (c != '_') r += c;
    return r;
}

inline void TomlParser::validate_underscores(std::string_view num, size_t line) {
    if (num.empty()) return;
    if (num.front() == '_' || num.back() == '_')
        error("underscore at start or end of number", line);
    for (size_t k = 1; k < num.size(); ++k) {
        if (num[k] == '_' && num[k-1] == '_')
            error("consecutive underscores in number", line);
    }
    for (size_t k = 0; k < num.size(); ++k) {
        if (num[k] != '_') continue;
        char prev = (k > 0) ? num[k-1] : '\0';
        char next = (k + 1 < num.size()) ? num[k+1] : '\0';
        if (prev == '+' || prev == '-' || prev == '.' || prev == 'e' || prev == 'E')
            error("underscore after '" + std::string(1, prev) + "' in number", line);
        if (next == '.' || next == 'e' || next == 'E' || next == '+' || next == '-')
            error("underscore before '" + std::string(1, next) + "' in number", line);
    }
}

inline TomlTable* TomlParser::get_as_table(TomlNode& node, const std::string& context, size_t line) {
    auto* ptr = std::get_if<std::shared_ptr<TomlTable>>(&node.val);
    if (!ptr) error(context + " is not a table", line);
    return ptr->get();
}

inline TomlTable* TomlParser::ensure_table(TomlTable* parent, const std::string& key, size_t line) {
    auto it = parent->find(key);
    if (it == parent->end()) {
        auto m = std::make_shared<TomlTable>();
        (*parent)[key] = TomlNode{TomlNode::Type::Table, m};
        return m.get();
    }
    auto* ptr = std::get_if<std::shared_ptr<TomlTable>>(&it->second.val);
    if (!ptr)
        error("key '" + key + "' already exists and is not a table", line);
    return ptr->get();
}

inline TomlTable* TomlParser::navigate_to_parent(
    TomlTable* root, const std::vector<std::string>& keys, bool through_arrays, size_t line) {
    if (keys.size() <= 1) return root;
    auto* cur = root;
    for (size_t k = 0; k < keys.size() - 1; ++k) {
        const auto& key = keys[k];
        auto it = cur->find(key);
        if (it == cur->end()) {
            auto m = std::make_shared<TomlTable>();
            (*cur)[key] = TomlNode{TomlNode::Type::Table, m};
            cur = m.get();
        } else {
            auto* tbl_ptr = std::get_if<std::shared_ptr<TomlTable>>(&it->second.val);
            if (tbl_ptr) {
                cur = tbl_ptr->get();
            } else if (through_arrays) {
                auto* arr_ptr = std::get_if<std::shared_ptr<TomlArray>>(&it->second.val);
                if (arr_ptr && !(*arr_ptr)->empty()) {
                    cur = get_as_table((*arr_ptr)->back(),
                        "last element of array '" + key + "'", line);
                } else {
                    error("key '" + key + "' is not a table or table array", line);
                }
            } else {
                error("key '" + key + "' is not a table", line);
            }
        }
    }
    return cur;
}

// ── Unicode 支持 ────────────────────────────────────────────────────────────

inline char32_t TomlParser::parse_unicode_escape(Ctx& ctx, int digits) {
    char32_t cp = 0;
    for (int d = 0; d < digits; ++d) {
        if (ctx.i >= ctx.s.size())
            error("incomplete Unicode escape sequence", ctx.line);
        char c = ctx.s[ctx.i++];
        cp <<= 4;
        if (c >= '0' && c <= '9') cp += c - '0';
        else if (c >= 'a' && c <= 'f') cp += c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') cp += c - 'A' + 10;
        else error("invalid hex digit in Unicode escape", ctx.line);
    }
    return cp;
}

inline void TomlParser::append_utf8(std::string& val, char32_t cp) {
    if (cp <= 0x7F) {
        val += static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        val += static_cast<char>(0xC0 | (cp >> 6));
        val += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        val += static_cast<char>(0xE0 | (cp >> 12));
        val += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        val += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0x10FFFF) {
        val += static_cast<char>(0xF0 | (cp >> 18));
        val += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        val += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        val += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

inline void TomlParser::parse_string_escape(Ctx& ctx, std::string& val) {
    ++ctx.i; // skip backslash
    if (ctx.i >= ctx.s.size())
        error("unexpected end of string after backslash", ctx.line);
    char c = ctx.s[ctx.i];
    if (c == 'u') {
        ++ctx.i;
        char32_t cp = parse_unicode_escape(ctx, 4);
        if (cp >= 0xD800 && cp <= 0xDFFF)
            error("surrogate codepoint in \\u escape", ctx.line);
        append_utf8(val, cp);
    } else if (c == 'U') {
        ++ctx.i;
        char32_t cp = parse_unicode_escape(ctx, 8);
        if (cp >= 0xD800 && cp <= 0xDFFF)
            error("surrogate codepoint in \\U escape", ctx.line);
        if (cp > 0x10FFFF)
            error("codepoint out of range in \\U escape", ctx.line);
        append_utf8(val, cp);
    } else {
        ++ctx.i;
        switch (c) {
            case 'n': val += '\n'; break;
            case 't': val += '\t'; break;
            case 'r': val += '\r'; break;
            case '"': val += '"'; break;
            case '\\': val += '\\'; break;
            case 'b': val += '\b'; break;
            case 'f': val += '\f'; break;
            default:
                error(std::string("invalid escape sequence: \\") + c, ctx.line);
                break;
        }
    }
}

// ── 解析简单键 ──────────────────────────────────────────────────────────────

inline std::string TomlParser::parse_simple_key(Ctx& ctx) {
    // 双引号键
    if (ctx.i < ctx.s.size() && ctx.s[ctx.i] == '"') {
        ++ctx.i;
        std::string key;
        while (ctx.i < ctx.s.size() && ctx.s[ctx.i] != '"') {
            if (ctx.s[ctx.i] == '\\' && ctx.i + 1 < ctx.s.size()) {
                parse_string_escape(ctx, key);
            } else {
                key += ctx.s[ctx.i++];
            }
        }
        if (ctx.i < ctx.s.size()) ++ctx.i;
        return key;
    }
    // 单引号键（字面量）
    if (ctx.i < ctx.s.size() && ctx.s[ctx.i] == '\'') {
        ++ctx.i;
        std::string key;
        while (ctx.i < ctx.s.size() && ctx.s[ctx.i] != '\'') key += ctx.s[ctx.i++];
        if (ctx.i < ctx.s.size()) ++ctx.i;
        return key;
    }
    // 裸键: [A-Za-z0-9_-]+
    std::string key;
    while (ctx.i < ctx.s.size() && ((ctx.s[ctx.i] >= 'a' && ctx.s[ctx.i] <= 'z') ||
           (ctx.s[ctx.i] >= 'A' && ctx.s[ctx.i] <= 'Z') ||
           (ctx.s[ctx.i] >= '0' && ctx.s[ctx.i] <= '9') ||
           ctx.s[ctx.i] == '_' || ctx.s[ctx.i] == '-')) {
        key += ctx.s[ctx.i++];
    }
    if (key.empty())
        error("empty bare key", ctx.line);
    return key;
}

// ── 解析值 ──────────────────────────────────────────────────────────────────

inline TomlNode TomlParser::parse_value(Ctx& ctx, bool allow_comments) {
    auto& s = ctx.s;
    auto& i = ctx.i;

    // 选择跳空白函数
    auto do_skip_ws_between = [&]() {
        if (allow_comments) skip_ws_nl_comments(s, i, ctx.line);
        else skip_ws_nl(s, i, ctx.line);
    };

    skip_ws(s, i);
    if (i >= s.size()) error("unexpected end of input while parsing value", ctx.line);

    char c = s[i];

    // ── 多行基本字符串 """...""" ──
    if (c == '"' && i + 2 < s.size() && s[i+1] == '"' && s[i+2] == '"') {
        i += 3;
        // 规范：开头 """ 后紧跟的第一个换行被忽略
        if (i < s.size() && s[i] == '\n') { ++i; ++ctx.line; }
        else if (i + 1 < s.size() && s[i] == '\r' && s[i+1] == '\n') { i += 2; ++ctx.line; }

        std::string val;
        while (i < s.size()) {
            if (s[i] == '"' && i + 2 < s.size() && s[i+1] == '"' && s[i+2] == '"') {
                i += 3; break;
            }
            if (s[i] == '\\') {
                // 检查行末反斜杠续行：\ 后跟可选空白再跟换行
                size_t k = i + 1;
                while (k < s.size() && (s[k] == ' ' || s[k] == '\t')) ++k;
                if (k < s.size() && s[k] == '\n') {
                    i = k + 1; ++ctx.line; continue;
                }
                if (k + 1 < s.size() && s[k] == '\r' && s[k+1] == '\n') {
                    i = k + 2; ++ctx.line; continue;
                }
                // 普通转义序列（含 Unicode）
                parse_string_escape(ctx, val);
                continue;
            }
            if (s[i] == '\n') { ++ctx.line; ++i; }
            else if (s[i] == '\r') {
                if (i + 1 < s.size() && s[i+1] == '\n') {
                    ++i; // 跳过 \r，下轮处理 \n
                } else {
                    val += s[i++]; // 单独 \r 当作普通字符
                }
            } else {
                val += s[i++];
            }
        }
        return TomlNode{TomlNode::Type::String, val};
    }

    // ── 基本字符串 "..." ──
    if (c == '"') {
        ++i;
        std::string val;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i + 1 < s.size()) {
                parse_string_escape(ctx, val);
            } else {
                val += s[i++];
            }
        }
        if (i < s.size()) ++i;
        return TomlNode{TomlNode::Type::String, val};
    }

    // ── 多行字面量字符串 '''...''' ──
    if (c == '\'' && i + 2 < s.size() && s[i+1] == '\'' && s[i+2] == '\'') {
        i += 3;
        // 规范：开头 ''' 后紧跟的第一个换行被忽略
        if (i < s.size() && s[i] == '\n') { ++i; ++ctx.line; }
        else if (i + 1 < s.size() && s[i] == '\r' && s[i+1] == '\n') { i += 2; ++ctx.line; }

        std::string val;
        while (i < s.size()) {
            if (s[i] == '\'' && i + 2 < s.size() && s[i+1] == '\'' && s[i+2] == '\'') {
                i += 3; break;
            }
            if (s[i] == '\n') { ++ctx.line; ++i; }
            else if (s[i] == '\r') {
                if (i + 1 < s.size() && s[i+1] == '\n') {
                    ++i; // 跳过 \r，下轮处理 \n
                } else {
                    val += s[i++]; // 单独 \r 当作普通字符
                }
            } else {
                val += s[i++];
            }
        }
        return TomlNode{TomlNode::Type::String, val};
    }

    // ── 字面量字符串 '...' ──
    if (c == '\'') {
        ++i;
        std::string val;
        while (i < s.size() && s[i] != '\'') val += s[i++];
        if (i < s.size()) ++i;
        return TomlNode{TomlNode::Type::String, val};
    }

    // ── 内联表 { ... } ──
    // TOML 规范：内联表内不允许 # 注释
    if (c == '{') {
        ++i;
        auto tbl = std::make_shared<TomlTable>();
        skip_ws_nl(s, i, ctx.line);
        if (i < s.size() && s[i] == '}') { ++i; return TomlNode{TomlNode::Type::Table, tbl}; }
        while (i < s.size()) {
            skip_ws_nl(s, i, ctx.line);
            if (i >= s.size()) break;
            // 禁止注释
            if (s[i] == '#') error("comments are not allowed inside inline tables", ctx.line);
            // 解析键（支持点分隔）
            std::vector<std::string> key_parts;
            key_parts.push_back(parse_simple_key(ctx));
            skip_ws(s, i);
            while (i < s.size() && s[i] == '.') {
                ++i; skip_ws(s, i);
                key_parts.push_back(parse_simple_key(ctx));
                skip_ws(s, i);
            }
            skip_ws(s, i);
            if (i < s.size() && s[i] == '=') ++i;
            else error("expected '=' in inline table", ctx.line);
            TomlNode value = parse_value(ctx, false);
            auto* target = tbl.get();
            for (size_t k = 0; k + 1 < key_parts.size(); ++k)
                target = ensure_table(target, key_parts[k], ctx.line);
            (*target)[key_parts.back()] = std::move(value);
            skip_ws_nl(s, i, ctx.line);
            if (i < s.size() && s[i] == ',') {
                ++i; skip_ws_nl(s, i, ctx.line);
                if (i < s.size() && s[i] == '}')
                    error("trailing comma in inline table is not allowed", ctx.line);
            }
            if (i < s.size() && s[i] == '}') { ++i; break; }
        }
        return TomlNode{TomlNode::Type::Table, tbl};
    }

    // ── 数组 [...] ──
    if (c == '[') {
        ++i;
        auto arr = std::make_shared<TomlArray>();
        do_skip_ws_between();
        while (i < s.size() && s[i] != ']') {
            size_t prev = i;
            arr->push_back(parse_value(ctx, true));
            if (i == prev)
                error("array parsing stuck", ctx.line);
            do_skip_ws_between();
            if (i < s.size() && s[i] == ',') ++i;
            do_skip_ws_between();
        }
        if (i < s.size()) ++i;
        return TomlNode{TomlNode::Type::Array, arr};
    }

    // ── 布尔值 ──
    if (c == 't' && i + 4 <= s.size() && s[i+1] == 'r' && s[i+2] == 'u' && s[i+3] == 'e') {
        i += 4; return TomlNode{TomlNode::Type::Bool, true};
    }
    if (c == 'f' && i + 5 <= s.size() && s[i+1] == 'a' && s[i+2] == 'l' && s[i+3] == 's' && s[i+4] == 'e') {
        i += 5; return TomlNode{TomlNode::Type::Bool, false};
    }

    // ── 无符号 inf / nan ──
    if (c == 'i' && i + 3 <= s.size() && s[i+1] == 'n' && s[i+2] == 'f') {
        i += 3; return TomlNode{TomlNode::Type::Float, INFINITY};
    }
    if (c == 'n' && i + 3 <= s.size() && s[i+1] == 'a' && s[i+2] == 'n') {
        i += 3; return TomlNode{TomlNode::Type::Float, NAN};
    }

    // ── 带符号 inf / nan ──
    if ((c == '+' || c == '-') && i + 4 <= s.size()) {
        if (s[i+2] == 'n' && s[i+3] == 'f') {
            bool neg = (c == '-'); i += 4;
            return TomlNode{TomlNode::Type::Float, neg ? -INFINITY : INFINITY};
        }
        if (s[i+1] == 'n' && s[i+2] == 'a' && s[i+3] == 'n') {
            bool neg = (c == '-'); i += 4;
            return TomlNode{TomlNode::Type::Float, neg ? -NAN : NAN};
        }
    }

    // ── 数字（整数 / 浮点数）──
    if (c == '+' || c == '-' || (c >= '0' && c <= '9')) {
        bool negative = (c == '-');
        size_t start = i;
        if (c == '+' || c == '-') ++i;

        // 十六进制 / 八进制 / 二进制
        if (i < s.size() && s[i] == '0' && i + 1 < s.size()) {
            char p = s[i + 1];
            if (p == 'x' || p == 'X') {
                i += 2; size_t j = i;
                while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') ||
                       (s[i] >= 'a' && s[i] <= 'f') || (s[i] >= 'A' && s[i] <= 'F') || s[i] == '_')) ++i;
                if (i < s.size() && (s[i] == '.' || s[i] == 'e' || s[i] == 'E'))
                    error("hexadecimal number cannot have fractional or exponent part", ctx.line);
                // 就地校验下划线位置（避免 substr 拷贝）
                {
                    size_t len = i - j;
                    if (len > 0 && s[j] == '_')
                        error("underscore at start of hex digits", ctx.line);
                    if (len > 0 && s[i-1] == '_')
                        error("underscore at end of hex digits", ctx.line);
                    for (size_t k = j + 1; k < i; ++k) {
                        if (s[k] == '_' && s[k-1] == '_')
                            error("consecutive underscores in hex number", ctx.line);
                    }
                }
                std::string h = strip_underscores(std::string_view(s.data() + j, i - j));
                if (h.empty()) error("invalid hex number", ctx.line);
                // 使用 uint64_t 中间解析，避免溢出
                uint64_t uv = 0;
                for (char ch : h) {
                    int d;
                    if (ch >= '0' && ch <= '9') d = ch - '0';
                    else if (ch >= 'a' && ch <= 'f') d = ch - 'a' + 10;
                    else d = ch - 'A' + 10;
                    if (uv > (uint64_t(std::numeric_limits<int64_t>::max()) * 2 - d) / 16)
                        error("integer overflow in hex number", ctx.line);
                    uv = uv * 16 + d;
                }
                if (negative) {
                    if (uv > uint64_t(std::numeric_limits<int64_t>::max()) + 1)
                        error("integer overflow in hex number", ctx.line);
                    if (uv == uint64_t(std::numeric_limits<int64_t>::max()) + 1)
                        return TomlNode{TomlNode::Type::Int, std::numeric_limits<int64_t>::min()};
                    return TomlNode{TomlNode::Type::Int, -static_cast<int64_t>(uv)};
                }
                if (uv > uint64_t(std::numeric_limits<int64_t>::max()))
                    error("integer overflow in hex number", ctx.line);
                return TomlNode{TomlNode::Type::Int, static_cast<int64_t>(uv)};
            }
            if (p == 'o' || p == 'O') {
                i += 2; size_t j = i;
                while (i < s.size() && ((s[i] >= '0' && s[i] <= '7') || s[i] == '_')) ++i;
                if (i < s.size() && (s[i] == '.' || s[i] == 'e' || s[i] == 'E'))
                    error("octal number cannot have fractional or exponent part", ctx.line);
                {
                    size_t len = i - j;
                    if (len > 0 && s[j] == '_')
                        error("underscore at start of octal digits", ctx.line);
                    if (len > 0 && s[i-1] == '_')
                        error("underscore at end of octal digits", ctx.line);
                    for (size_t k = j + 1; k < i; ++k) {
                        if (s[k] == '_' && s[k-1] == '_')
                            error("consecutive underscores in octal number", ctx.line);
                    }
                }
                std::string o = strip_underscores(std::string_view(s.data() + j, i - j));
                if (o.empty()) error("invalid octal number", ctx.line);
                uint64_t uv = 0;
                for (char ch : o) {
                    int d = ch - '0';
                    if (uv > (uint64_t(std::numeric_limits<int64_t>::max()) * 2 - d) / 8)
                        error("integer overflow in octal number", ctx.line);
                    uv = uv * 8 + d;
                }
                if (negative) {
                    if (uv > uint64_t(std::numeric_limits<int64_t>::max()) + 1)
                        error("integer overflow in octal number", ctx.line);
                    if (uv == uint64_t(std::numeric_limits<int64_t>::max()) + 1)
                        return TomlNode{TomlNode::Type::Int, std::numeric_limits<int64_t>::min()};
                    return TomlNode{TomlNode::Type::Int, -static_cast<int64_t>(uv)};
                }
                if (uv > uint64_t(std::numeric_limits<int64_t>::max()))
                    error("integer overflow in octal number", ctx.line);
                return TomlNode{TomlNode::Type::Int, static_cast<int64_t>(uv)};
            }
            if (p == 'b' || p == 'B') {
                i += 2; size_t j = i;
                while (i < s.size() && ((s[i] == '0' || s[i] == '1') || s[i] == '_')) ++i;
                if (i < s.size() && (s[i] == '.' || s[i] == 'e' || s[i] == 'E'))
                    error("binary number cannot have fractional or exponent part", ctx.line);
                {
                    size_t len = i - j;
                    if (len > 0 && s[j] == '_')
                        error("underscore at start of binary digits", ctx.line);
                    if (len > 0 && s[i-1] == '_')
                        error("underscore at end of binary digits", ctx.line);
                    for (size_t k = j + 1; k < i; ++k) {
                        if (s[k] == '_' && s[k-1] == '_')
                            error("consecutive underscores in binary number", ctx.line);
                    }
                }
                std::string b = strip_underscores(std::string_view(s.data() + j, i - j));
                if (b.empty()) error("invalid binary number", ctx.line);
                uint64_t uv = 0;
                for (char ch : b) {
                    int d = ch - '0';
                    if (uv > (uint64_t(std::numeric_limits<int64_t>::max()) * 2 - d) / 2)
                        error("integer overflow in binary number", ctx.line);
                    uv = uv * 2 + d;
                }
                if (negative) {
                    if (uv > uint64_t(std::numeric_limits<int64_t>::max()) + 1)
                        error("integer overflow in binary number", ctx.line);
                    if (uv == uint64_t(std::numeric_limits<int64_t>::max()) + 1)
                        return TomlNode{TomlNode::Type::Int, std::numeric_limits<int64_t>::min()};
                    return TomlNode{TomlNode::Type::Int, -static_cast<int64_t>(uv)};
                }
                if (uv > uint64_t(std::numeric_limits<int64_t>::max()))
                    error("integer overflow in binary number", ctx.line);
                return TomlNode{TomlNode::Type::Int, static_cast<int64_t>(uv)};
            }
        }

        // 十进制（整数或浮点数）
        while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '_')) ++i;
        bool is_float = false;
        if (i < s.size() && s[i] == '.') {
            is_float = true; ++i;
            while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '_')) ++i;
        }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            is_float = true; ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '_')) ++i;
        }

        std::string_view raw_num(s.data() + start, i - start);
        // 校验下划线位置
        validate_underscores(raw_num, ctx.line);
        std::string num_str = strip_underscores(raw_num);

        if (is_float) {
            double v = 0;
            std::sscanf(num_str.c_str(), "%lf", &v);
            return TomlNode{TomlNode::Type::Float, v};
        }
        // 十进制整数禁止前导零（0 本身除外）
        if (num_str.size() > 1 && num_str[0] == '0')
            error("decimal integer cannot have leading zeroes", ctx.line);
        try {
            int64_t v = std::stoll(num_str);
            return TomlNode{TomlNode::Type::Int, v};
        } catch (const std::out_of_range&) {
            error("integer overflow: " + num_str, ctx.line);
        } catch (const std::invalid_argument&) {
            error("invalid number: " + num_str, ctx.line);
        }
    }

    error(std::string("unexpected character in value: '") + c + "'", ctx.line);
}

// ── 主解析函数 ──────────────────────────────────────────────────────────────

inline TomlNode TomlParser::parse(std::string_view content) {
    std::string s(content);

    // 处理 UTF-8 BOM
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF &&
        (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) {
        s.erase(0, 3);
    }

    auto root = std::make_shared<TomlTable>();
    TomlTable* cur = root.get();

    size_t i = 0;
    size_t line = 1;

    while (i < s.size()) {
        skip_ws_nl_comments(s, i, line);
        if (i >= s.size()) break;

        // ── 表数组头 [[...]] ──
        if (s[i] == '[' && i + 1 < s.size() && s[i+1] == '[') {
            i += 2;
            skip_ws(s, i);
            Ctx ctx(s, i);
            ctx.line = line;
            std::vector<std::string> keys;
            keys.push_back(parse_simple_key(ctx));
            i = ctx.i; line = ctx.line;
            skip_ws(s, i);
            while (i < s.size() && s[i] == '.') {
                ++i; skip_ws(s, i);
                Ctx ctx2(s, i);
                ctx2.line = line;
                keys.push_back(parse_simple_key(ctx2));
                i = ctx2.i; line = ctx2.line;
                skip_ws(s, i);
            }
            // 先定位 ]]，消耗结束标记
            while (i < s.size() && s[i] != ']') ++i;
            if (i + 1 < s.size() && s[i] == ']' && s[i+1] == ']') i += 2;
            else if (i < s.size()) ++i;

            // 检测 ] 后的多余字符（只允许注释）
            skip_ws(s, i);
            if (i < s.size() && s[i] == '#') {
                while (i < s.size() && s[i] != '\n') ++i;
            } else if (i < s.size() && s[i] != '\n' && s[i] != '\r') {
                error("extra characters after table array header", line);
            }

            auto* parent = navigate_to_parent(root.get(), keys, true, line);
            const std::string& last_key = keys.back();
            auto it = parent->find(last_key);
            if (it == parent->end()) {
                auto arr = std::make_shared<TomlArray>();
                auto m = std::make_shared<TomlTable>();
                arr->push_back(TomlNode{TomlNode::Type::Table, m});
                (*parent)[last_key] = TomlNode{TomlNode::Type::Array, arr};
                cur = m.get();
            } else {
                auto* arr_ptr = std::get_if<std::shared_ptr<TomlArray>>(&it->second.val);
                if (!arr_ptr) error("key '" + last_key + "' is not an array", line);
                auto m = std::make_shared<TomlTable>();
                (*arr_ptr)->push_back(TomlNode{TomlNode::Type::Table, m});
                cur = m.get();
            }
            continue;
        }

        // ── 表头 [...] ──
        if (s[i] == '[') {
            ++i;
            skip_ws(s, i);
            Ctx ctx(s, i);
            ctx.line = line;
            std::vector<std::string> keys;
            keys.push_back(parse_simple_key(ctx));
            i = ctx.i; line = ctx.line;
            skip_ws(s, i);
            while (i < s.size() && s[i] == '.') {
                ++i; skip_ws(s, i);
                Ctx ctx2(s, i);
                ctx2.line = line;
                keys.push_back(parse_simple_key(ctx2));
                i = ctx2.i; line = ctx2.line;
                skip_ws(s, i);
            }
            // 先定位 ]，消耗结束标记
            while (i < s.size() && s[i] != ']') ++i;
            if (i < s.size()) ++i;

            // 检测 ] 后的多余字符（只允许注释）
            skip_ws(s, i);
            if (i < s.size() && s[i] == '#') {
                while (i < s.size() && s[i] != '\n') ++i;
            } else if (i < s.size() && s[i] != '\n' && s[i] != '\r') {
                error("extra characters after table header", line);
            }

            auto* parent = navigate_to_parent(root.get(), keys, true, line);
            cur = ensure_table(parent, keys.back(), line);
            continue;
        }

        // ── key = value ──
        Ctx ctx(s, i);
        ctx.line = line;
        std::vector<std::string> key_parts;
        key_parts.push_back(parse_simple_key(ctx));
        i = ctx.i; line = ctx.line;
        skip_ws(s, i);
        while (i < s.size() && s[i] == '.') {
            ++i; skip_ws(s, i);
            Ctx ctx2(s, i);
            ctx2.line = line;
            key_parts.push_back(parse_simple_key(ctx2));
            i = ctx2.i; line = ctx2.line;
            skip_ws(s, i);
        }
        skip_ws(s, i);
        if (i >= s.size() || s[i] != '=') {
            error("expected '=' after key", line);
        }
        ++i; // skip '='

        Ctx val_ctx(s, i);
        val_ctx.line = line;
        TomlNode value = parse_value(val_ctx, false);
        i = val_ctx.i; line = val_ctx.line;

        auto* target = cur;
        for (size_t k = 0; k + 1 < key_parts.size(); ++k)
            target = ensure_table(target, key_parts[k], line);
        (*target)[key_parts.back()] = std::move(value);

        // 跳到行尾（跳过行内注释）
        while (i < s.size() && s[i] != '\n') {
            if (s[i] == '\r') { ++i; continue; }
            ++i;
        }
    }

    return TomlNode{TomlNode::Type::Table, root};
}

}  // namespace tangbase
