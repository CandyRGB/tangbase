#pragma once
// tangbase::DateTime — time point representation with formatting support
// Requires C++17 and <ctime>. Not thread-safe (formatting uses localtime).

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <string_view>

namespace tangbase {

// DateTime — represents a point in time as seconds since epoch (time_t).
// All formatting uses the local timezone. Thread-unsafe due to std::localtime.
class DateTime {
public:
    // Construct from a time_t value (UTC seconds since epoch).
    explicit DateTime(time_t t) : t_(t) {}

    // Return the current local DateTime.
    static DateTime now();

    // Return the current UTC DateTime.
    // Note: on Windows, this currently falls back to localtime (timegm not used).
    static DateTime utc_now();

    // Format this DateTime according to fmt.
    // Supported placeholders:
    //   %Y  year (e.g. 2026)   %m  month 01-12   %d  day 01-31
    //   %H  hour 00-23         %M  minute 00-59   %S  second 00-59
    //   %j  day of year 001-366  %w  weekday 0=Sun
    //   %%  literal %
    std::string format(std::string_view fmt) const;

    // Pre-defined formats.
    // to_iso8601() — "2026-05-14T10:30:00"
    // to_datetime() — "2026-05-14 10:30:00"
    // to_date()    — "2026-05-14"
    // to_time()    — "10:30:00"
    std::string to_iso8601() const;
    std::string to_datetime() const;
    std::string to_date() const;
    std::string to_time() const;

    // Return the underlying Unix timestamp (seconds since epoch).
    time_t epoch() const { return t_; }

    // Human-readable relative time from now. e.g. "5m ago", "2d ago".
    // Returns "in the future" if t_ is in the future relative to now.
    std::string ago() const;

private:
    time_t t_;
};

inline DateTime DateTime::now() {
    return DateTime(std::time(nullptr));
}

inline DateTime DateTime::utc_now() {
    auto now = std::time(nullptr);
    std::tm tm = {};
#if defined(_WIN32)
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
#if defined(_WIN32)
    return DateTime(_mkgmtime(&tm));
#else
    return DateTime(timegm(&tm));
#endif
}

inline std::string DateTime::format(std::string_view fmt) const {
    char buf[128];
    std::tm tm;
#if defined(_WIN32)
    localtime_s(&tm, &t_);
#else
    localtime_r(&t_, &tm);
#endif

    std::string result;
    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] != '%' || i + 1 >= fmt.size()) {
            result.push_back(fmt[i]);
            continue;
        }
        switch (fmt[i+1]) {
            case 'Y': std::snprintf(buf, sizeof(buf), "%04d", tm.tm_year + 1900); break;
            case 'm': std::snprintf(buf, sizeof(buf), "%02d", tm.tm_mon + 1);     break;
            case 'd': std::snprintf(buf, sizeof(buf), "%02d", tm.tm_mday);        break;
            case 'H': std::snprintf(buf, sizeof(buf), "%02d", tm.tm_hour);        break;
            case 'M': std::snprintf(buf, sizeof(buf), "%02d", tm.tm_min);         break;
            case 'S': std::snprintf(buf, sizeof(buf), "%02d", tm.tm_sec);          break;
            case 'j': std::snprintf(buf, sizeof(buf), "%03d", tm.tm_yday + 1);     break;
            case 'w': std::snprintf(buf, sizeof(buf), "%d",   tm.tm_wday);          break;
            case '%': buf[0] = '%'; buf[1] = '\0'; break;
            default:  buf[0] = fmt[i+1]; buf[1] = '\0'; break;
        }
        result += buf;
        ++i;
    }
    return result;
}

inline std::string DateTime::to_iso8601() const { return format("%Y-%m-%dT%H:%M:%S"); }
inline std::string DateTime::to_datetime() const { return format("%Y-%m-%d %H:%M:%S"); }
inline std::string DateTime::to_date() const { return format("%Y-%m-%d"); }
inline std::string DateTime::to_time() const { return format("%H:%M:%S"); }

inline std::string DateTime::ago() const {
    auto now = std::time(nullptr);
    auto diff = now - t_;
    if (diff < 0) return "in the future";
    if (diff < 60)       return std::to_string(diff) + "s ago";
    if (diff < 3600)     return std::to_string(diff/60) + "m ago";
    if (diff < 86400)    return std::to_string(diff/3600) + "h ago";
    if (diff < 2592000)  return std::to_string(diff/86400) + "d ago";
    return std::to_string(diff/2592000) + " months ago";
}

}  // namespace tangbase