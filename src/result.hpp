#pragma once
// tangbase::Result<T, E> — Rust-style error handling
// tangbase::Option<T>   — alias for std::optional<T>

#include <optional>
#include <type_traits>
#include <variant>

namespace tangbase {

// Result<T, E> — represents either a success value (T) or an error (E).
// Used in place of exceptions for fallible operations.
// <T>          — success value type
// <E>          — error type (commonly std::error_code or int)
template<class T, class E>
class Result {
public:
    // Construct a success result.
    static Result ok(T value) {
        return Result(std::move(value), Tag::OK);
    }

    // Construct an error result.
    static Result err(E error) {
        return Result(std::move(error), Tag::ERR);
    }

    // true if this is a success result.
    bool is_ok() const { return std::holds_alternative<T>(data_); }

    // true if this is an error result.
    bool is_err() const { return std::holds_alternative<E>(data_); }

    // Pointer to the success value, or nullptr if error.
    const T* ok() const { return std::get_if<T>(&data_); }
    T* ok() { return std::get_if<T>(&data_); }

    // Pointer to the error value, or nullptr if success.
    const E* err() const { return std::get_if<E>(&data_); }
    E* err() { return std::get_if<E>(&data_); }

    // Return value if ok, otherwise return default_val.
    T unwrap_or(T default_val) const {
        if (auto* v = ok()) return *v;
        return default_val;
    }

    // Return error if err, otherwise return default_err.
    E unwrap_err_or(E default_err) const {
        if (auto* v = err()) return *v;
        return default_err;
    }

    // Transform the success value: Result<U, E> = r.map(f)
    // If this is err, returns err unchanged.
    template<class F, class U = std::invoke_result_t<F, const T&>>
    Result<U, E> map(F&& f) const {
        if (auto* v = ok()) {
            return Result<U, E>::ok(std::invoke(std::forward<F>(f), *v));
        }
        return Result<U, E>::err(*err());
    }

    // Transform the error value: Result<T, U> = r.map_err(f)
    // If this is ok, returns ok unchanged.
    template<class F, class U = std::invoke_result_t<F, const E&>>
    Result<T, U> map_err(F&& f) const {
        if (auto* v = err()) {
            return Result<T, U>::err(std::invoke(std::forward<F>(f), *v));
        }
        return Result<T, U>::ok(*ok());
    }

    // Chain error handling: if ok, call f(value); otherwise propagate error.
    // f must return a Result. Suitable for early return on error.
    template<class F, class U = std::invoke_result_t<F, const T&>>
    Result<U, E> and_then(F&& f) const {
        if (auto* v = ok()) {
            return std::invoke(std::forward<F>(f), *v);
        }
        return Result<U, E>::err(*err());
    }

    // Chain success handling: if err, call f(error); otherwise propagate value.
    // f must return a Result. Suitable for error recovery.
    template<class F, class U = std::invoke_result_t<F, const E&>>
    Result<T, U> or_else(F&& f) const {
        if (auto* v = err()) {
            return std::invoke(std::forward<F>(f), *v);
        }
        return Result<T, U>::ok(*ok());
    }

private:
    enum class Tag { OK, ERR };

    Result(T val, Tag) : data_(std::move(val)) {}
    Result(E val, Tag) : data_(std::move(val)) {}

    std::variant<T, E> data_;
};

// Option<T> — alias for std::optional<T>, representing an optional value.
// Use Result<T, E> when you need to express why a value is absent.
template<class T>
using Option = std::optional<T>;

}  // namespace tangbase