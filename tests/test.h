#pragma once

// A tiny unit-test framework, in the spirit of the rest of the project: no
// libraries, just enough to be pleasant to use.
//
//   TEST(cross_product_follows_right_hand_rule) {
//       CHECK_EQ(cross(x, y).z, 1.0f);
//   }
//
// CHECK_* macros record a failure and keep going, so one run shows every
// problem in a test. REQUIRE stops the test, for when carrying on would crash
// (e.g. indexing into a vector that turned out to be empty).

#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace test {

struct TestCase {
    const char* name;
    void (*run)();
};

// All tests, in the order they were registered. A function rather than a
// global variable: globals in different .cpp files are constructed in no
// particular order, and the registry must exist before the first TEST adds
// itself to it. A static local is created the first time it's used.
inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

// Adds a test to the registry. Each TEST creates one of these as a global,
// and its constructor runs before main(), so tests register themselves.
struct Registrar {
    Registrar(const char* name, void (*run)()) { registry().push_back({name, run}); }
};

// Failures in the test that's currently running. The runner resets it.
inline int& currentFailures() {
    static int count = 0;
    return count;
}

// Thrown by REQUIRE to stop the current test.
struct AbortTest {};

inline void fail(const char* file, int line, const std::string& message) {
    ++currentFailures();
    std::cout << "  " << file << ":" << line << ": " << message << "\n";
}

// Detects whether `std::ostream << value` compiles for a type, so failure
// messages can show values where possible.
template <typename T, typename = void>
struct IsPrintable : std::false_type {};
template <typename T>
struct IsPrintable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

// Turns a value into text for a failure message.
template <typename T>
std::string describe(const T& value) {
    std::ostringstream out;
    if constexpr (std::is_same_v<T, std::string>) {
        out << '"' << value << '"';
    } else if constexpr (std::is_enum_v<T>) {
        // enum class values can't be printed; show the number behind them.
        out << "enum value " << static_cast<long long>(value);
    } else if constexpr (IsPrintable<T>::value) {
        out << value;
    } else {
        out << "(a value that can't be printed)";
    }
    return out.str();
}

template <typename A, typename B>
void checkEqual(const A& a, const B& b, const char* aText, const char* bText,
                const char* file, int line) {
    if (!(a == b)) {
        fail(file, line, std::string("CHECK_EQ(") + aText + ", " + bText + ")\n" +
                             "      left:  " + describe(a) + "\n" +
                             "      right: " + describe(b));
    }
}

inline void checkNear(double a, double b, double tolerance, const char* aText,
                      const char* bText, const char* file, int line) {
    const double diff = a > b ? a - b : b - a;
    if (!(diff <= tolerance)) { // written this way so NaN fails too
        fail(file, line, std::string("CHECK_NEAR(") + aText + ", " + bText + ")\n" +
                             "      left:  " + describe(a) + "\n" +
                             "      right: " + describe(b) + "\n" +
                             "      allowed difference: " + describe(tolerance));
    }
}

} // namespace test

// Defines a test. The name must be a valid identifier and unique in its file.
// It expands to: declare the function, register it, then define it, so the
// `{ ... }` written after TEST(name) becomes the function's body.
#define TEST(name)                                                    \
    static void name();                                               \
    static const test::Registrar name##_registrar(#name, name);       \
    static void name()

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) test::fail(__FILE__, __LINE__, "CHECK(" #cond ")"); \
    } while (0)

#define REQUIRE(cond)                                                 \
    do {                                                              \
        if (!(cond)) {                                                \
            test::fail(__FILE__, __LINE__, "REQUIRE(" #cond ")");     \
            throw test::AbortTest{};                                  \
        }                                                             \
    } while (0)

#define CHECK_EQ(a, b) test::checkEqual((a), (b), #a, #b, __FILE__, __LINE__)

#define CHECK_NEAR(a, b, tolerance) \
    test::checkNear((a), (b), (tolerance), #a, #b, __FILE__, __LINE__)

// Checks that `expr` throws `ExceptionType`, and that its message contains
// `text` (pass "" to accept any message).
#define CHECK_THROWS(ExceptionType, expr, text)                                  \
    do {                                                                         \
        try {                                                                    \
            (void)(expr);                                                        \
            test::fail(__FILE__, __LINE__, "expected " #ExceptionType " from " #expr); \
        } catch (const ExceptionType& e) {                                       \
            if (std::string(e.what()).find(text) == std::string::npos) {         \
                test::fail(__FILE__, __LINE__,                                   \
                           std::string("message of " #expr " should contain \"") + \
                               (text) + "\"\n      but was: \"" + e.what() + "\""); \
            }                                                                    \
        } catch (const std::exception& e) {                                      \
            test::fail(__FILE__, __LINE__,                                       \
                       std::string("expected " #ExceptionType " from " #expr     \
                                   ", got another exception: ") + e.what());     \
        }                                                                        \
    } while (0)
