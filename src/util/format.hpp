#pragma once
// tangbase::format(fmt, args...) — Python-style {} placeholder formatting
// Requires C++17. Throws tangbase::format_error on malformed format strings.
// Supports: int, long, long long, unsigned, unsigned long, unsigned long long,
//           float, double, bool, char, const char*, std::string, std::string_view

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>

namespace tangbase {

class format_error : public std::exception {
public:
    explicit format_error(const char* msg) : std::exception(msg) {}
};

namespace detail {

template<class T>
void write_arg(std::string& out, const T& val) {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        out.append(val);
    } else if constexpr (std::is_same_v<std::decay_t<T>, std::string_view>) {
        out.append(val.data(), val.size());
    } else if constexpr (std::is_same_v<std::decay_t<T>, const char*> ||
                          std::is_same_v<std::decay_t<T>, char*>) {
        out.append(val ? val : "(null)");
    } else if constexpr (std::is_same_v<std::decay_t<T>, char>) {
        out.push_back(val);
    } else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        out.append(val ? "true" : "false");
    } else if constexpr (std::is_same_v<std::decay_t<T>, int>) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", val);
        out.append(buf);
    } else if constexpr (std::is_same_v<std::decay_t<T>, long>) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", val);
        out.append(buf);
    } else if constexpr (std::is_same_v<std::decay_t<T>, long long>) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", val);
        out.append(buf);
    } else if constexpr (std::is_same_v<std::decay_t<T>, unsigned>) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u", val);
        out.append(buf);
    } else if constexpr (std::is_same_v<std::decay_t<T>, unsigned long>) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lu", val);
        out.append(buf);
    } else if constexpr (std::is_same_v<std::decay_t<T>, unsigned long long>) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%llu", val);
        out.append(buf);
    } else if constexpr (std::is_same_v<std::decay_t<T>, float>) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.7g", val);
        out.append(buf);
    } else if constexpr (std::is_same_v<std::decay_t<T>, double>) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.15g", val);
        out.append(buf);
    } else {
        out.append("<unknown>");
    }
}

template<size_t I, size_t N, class... Ts>
void format_impl_arg(std::string& out, size_t idx, const std::tuple<Ts...>& t) {
    if constexpr (I < N) {
        if (I == idx) {
            write_arg(out, std::get<I>(t));
        } else {
            format_impl_arg<I + 1, N>(out, idx, t);
        }
    }
}

template<class... Ts>
void format_impl(std::string& out, std::string_view fmt, const std::tuple<Ts...>& args_tuple) {
    size_t placeholder_count = 0;
    size_t i = 0;
    constexpr size_t N = sizeof...(Ts);

    while (i < fmt.size()) {
        if (i < fmt.size() - 1 && fmt[i] == '{' && fmt[i + 1] == '{') {
            out.push_back('{');
            i += 2;
            continue;
        }
        if (i < fmt.size() - 1 && fmt[i] == '}' && fmt[i + 1] == '}') {
            out.push_back('}');
            i += 2;
            continue;
        }
        if (fmt[i] == '{') {
            size_t close = fmt.find('}', i);
            if (close == std::string_view::npos) {
                throw format_error("unmatched '{'");
            }
            if (close != i + 1) {
                throw format_error("only empty {} placeholders are supported");
            }
            if (placeholder_count >= N) {
                throw format_error("too few arguments");
            }
            format_impl_arg<0, N>(out, placeholder_count, args_tuple);
            placeholder_count++;
            i = close + 1;
            continue;
        }
        if (fmt[i] == '}') {
            throw format_error("unexpected '}'");
        }
        out.push_back(fmt[i]);
        i++;
    }
}

}  // namespace detail

template<class... Args>
std::string format(std::string_view fmt, Args&&... args) {
    std::string out;
    auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);
    detail::format_impl(out, fmt, args_tuple);
    return out;
}

}  // namespace tangbase