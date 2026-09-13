#pragma once

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdate-time"
#endif

namespace koto {

inline constexpr const char* kVersion = "0.1.0";
inline constexpr const char* kDeviceName = "kotoproto-os";
inline constexpr const char* kCompileTimestamp = __DATE__;

}  // namespace koto

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
