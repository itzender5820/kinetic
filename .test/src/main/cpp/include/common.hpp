#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  include/common.hpp  —  Shared types, logging, C++20 utilities
// ─────────────────────────────────────────────────────────────────────────────
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <span>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <cmath>
#include <numbers>
#include <algorithm>
#include <concepts>
#include <android/log.h>

// ── Logging ───────────────────────────────────────────────────────────────────
#define APPTAG "TESTAPP"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  APPTAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, APPTAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, APPTAG, __VA_ARGS__)

// ── C++20 Concepts ────────────────────────────────────────────────────────────
namespace testapp {

template<typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template<typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

// Clamp a value between lo and hi
template<Arithmetic T>
constexpr T clamp(T v, T lo, T hi) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Linear interpolation
template<FloatingPoint T>
constexpr T lerp(T a, T b, T t) noexcept {
    return a + (b - a) * t;
}

// Map value from [in_lo,in_hi] to [out_lo,out_hi]
template<FloatingPoint T>
constexpr T map_range(T v, T in_lo, T in_hi, T out_lo, T out_hi) noexcept {
    return out_lo + (out_hi - out_lo) * ((v - in_lo) / (in_hi - in_lo));
}

} // namespace testapp
