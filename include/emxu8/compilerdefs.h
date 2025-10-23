#pragma once
#include <type_traits>
#include <climits>

#if defined(_WIN32) || defined(_WIN64)
#ifdef BUILDING_EMXU8
#define EMXU8_API __declspec(dllexport)
#else
#define EMXU8_API __declspec(dllimport)
#endif
#else
#define EMXU8_API __attribute__((visibility("default")))
#endif

// Packed structs
#if defined(_MSC_VER)
#define PACKED_STRUCT(name) __pragma(pack(push, 1)) struct name __pragma(pack(pop))
#else
#define PACKED_STRUCT(name) struct __attribute__((packed)) name
#endif

// Packed anonymous structs
#if defined(_MSC_VER)
#define PACKED_ANON_STRUCT \
    __pragma(pack(push,1)) struct __pragma(pack(pop))
#else
#define PACKED_ANON_STRUCT \
    struct __attribute__((packed))
#endif

// ChatGPT
inline void debug_break() {
#if defined(_MSC_VER)
	__debugbreak();
#elif defined(__clang__) || defined(__GNUC__)
	__builtin_trap();
#else
	std::abort();
#endif
}

// ChatGPT
template<typename T>
constexpr auto sign_extend(T x, unsigned int bits) -> std::make_signed_t<T> {
	using S = std::make_signed_t<T>;
	constexpr int width = sizeof(T) * CHAR_BIT;

	if (bits == 0) return 0;
	if (bits >= width) return static_cast<S>(x);

	// Shift left into the sign bit of a signed type, then arithmetic shift right
	S sx = static_cast<S>(x << (width - bits));
	return sx >> (width - bits);
}
