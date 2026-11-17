#pragma once
#include <cstdarg>
#include <cstdint>

namespace o32x {

enum class LogLevel { Error = 0, Warn = 1, Info = 2, Debug = 3, Trace = 4 };

void log_set_level(LogLevel lvl);
LogLevel log_get_level();
void log_msg(LogLevel lvl, const char* category, const char* fmt, ...)
#if defined(__MINGW32__) && !defined(__clang__)
    __attribute__((format(gnu_printf, 3, 4)))
#elif defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

// Rate-limited logging for unimplemented hardware accesses: prints the first
// few occurrences per call site.
#define O32X_LOG_LIMITED(lvl, cat, max, ...)                          \
    do {                                                                 \
        static int o32x_log_count_ = 0;                               \
        if (o32x_log_count_ < (max)) {                                \
            ++o32x_log_count_;                                        \
            ::o32x::log_msg(lvl, cat, __VA_ARGS__);                   \
        }                                                                \
    } while (0)

#define LOGE(cat, ...) ::o32x::log_msg(::o32x::LogLevel::Error, cat, __VA_ARGS__)
#define LOGW(cat, ...) ::o32x::log_msg(::o32x::LogLevel::Warn, cat, __VA_ARGS__)
#define LOGI(cat, ...) ::o32x::log_msg(::o32x::LogLevel::Info, cat, __VA_ARGS__)
#define LOGD(cat, ...) do { if (::o32x::log_get_level() >= ::o32x::LogLevel::Debug) ::o32x::log_msg(::o32x::LogLevel::Debug, cat, __VA_ARGS__); } while (0)

} // namespace o32x
