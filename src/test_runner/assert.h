#ifndef TEST_RUNNER_ASSERT_H_
#define TEST_RUNNER_ASSERT_H_

#include <cstddef>
#include <vector>
#include "fmt/base.h"
#include "fmt/ranges.h"

inline void _assertTrueImpl(bool value, const char* expressionText, const char* file, int line)
{
	if (!value)
		throw fmt::format("Assertion failed at {}:{}\n  Expression: {}", file, line, expressionText);
}

// 2. The macro captures the expression text (#expr) and location
#define assertTrue(expr) _assertTrueImpl((expr), #expr, __FILE__, __LINE__)

template <typename T>
inline void assertSize(const T& v, size_t expected)
{
	if (v.size() != expected)
		throw fmt::format("expected size {} but got {}", expected, v.size());
}

template<typename T>
concept IsEnum = std::is_enum_v<T>;

template <typename T> requires (!IsEnum<T>)
inline void assertEqual(const T& v, const T& expected)
{
	if (v != expected)
		throw fmt::format("expected:\n{}\n  but got:\n{}", expected, v);
}

template<IsEnum T>
inline void assertEqual(T v, T expected)
{
	if (v != expected)
		throw fmt::format("expected:\n{}\n  but got:\n{}", (int)expected, (int)v);
}

inline void assertEqual(const std::vector<std::string>& v, const std::vector<std::string>& expected)
{
	if (v != expected)
		throw fmt::format("expected:\n{}\n  but got:\n{}", fmt::join(expected, ", "), fmt::join(v, ", "));
}

template <typename T>
inline void assertNotEqual(const T& v, const T& expected)
{
	if (v == expected)
		throw fmt::format("expected:\n{}\n  to not equal:\n{}", v, expected);
}

template <typename T>
inline void assertGreaterThan(const T& v, const T& expected)
{
	if (v <= expected)
		throw fmt::format("expected:\n{}\n  to be greater than:\n{}", v, expected);
}

template<typename Container, typename Predicate>
inline void assertSome(const Container& v, Predicate predicate)
{
	if (!std::any_of(v.begin(), v.end(), predicate))
		throw std::string("Assertion failed: No elements in the vector matched the predicate.");
}

#endif
