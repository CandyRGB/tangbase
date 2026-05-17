#pragma once
// tangbase::util::File — 文件读写工具函数
// read_file / write_file / append_file
// 依赖: tangbase::Result (src/result.hpp)

#include <cstdio>
#include <string>
#include <string_view>
#include <system_error>

#include <result.hpp>

namespace tangbase::util {

// 读取文件全部内容
// path  — 文件路径（UTF-8）
// 成功返回文件内容字符串；失败返回 std::error_code
// 错误码: std::errc::no_such_file_or_directory
inline auto read_file(std::string_view path) -> Result<std::string, std::error_code> {
    FILE* f = std::fopen(std::string(path).c_str(), "rb");
    if (!f) return Result<std::string, std::error_code>::err(
        std::make_error_code(std::errc::no_such_file_or_directory));

    std::string content;
    char buf[512];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        content.append(buf, n);
    }
    std::fclose(f);
    return Result<std::string, std::error_code>::ok(std::move(content));
}

// 将内容写入文件（覆盖模式）
// path     — 文件路径
// content  — 要写入的字符串
// 成功返回空；失败返回 std::error_code
// 错误码: std::errc::no_such_file_or_directory / std::errc::io_error
inline std::error_code write_file(std::string_view path, std::string_view content) {
    FILE* f = std::fopen(std::string(path).c_str(), "wb");
    if (!f) return std::make_error_code(std::errc::no_such_file_or_directory);

    size_t n = std::fwrite(content.data(), 1, content.size(), f);
    std::fclose(f);
    if (n != content.size()) return std::make_error_code(std::errc::io_error);
    return {};
}

// 追加内容到文件末尾
// path     — 文件路径
// content  — 要追加的字符串
// 成功返回空；失败返回 std::error_code
// 若文件不存在，会自动创建
inline std::error_code append_file(std::string_view path, std::string_view content) {
    FILE* f = std::fopen(std::string(path).c_str(), "ab");
    if (!f) return std::make_error_code(std::errc::no_such_file_or_directory);

    size_t n = std::fwrite(content.data(), 1, content.size(), f);
    std::fclose(f);
    if (n != content.size()) return std::make_error_code(std::errc::io_error);
    return {};
}

}  // namespace tangbase::util