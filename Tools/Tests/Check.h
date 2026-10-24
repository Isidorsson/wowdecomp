#ifndef TOOLS_TESTS_CHECK_H
#define TOOLS_TESTS_CHECK_H

#include <cstdio>
#include <vector>

// Dependency-free test registry: TEST(name) defines a case, CHECK(expr) records a failure and continues.
namespace Check {

struct Case {
    const char* name;
    void (*run)();
};

inline std::vector<Case>& Cases() {
    static std::vector<Case> cases;
    return cases;
}

inline int& Failures() {
    static int failures = 0;
    return failures;
}

struct Register {
    Register(const char* name, void (*run)()) { Cases().push_back({ name, run }); }
};

}

#define TEST(name) \
    static void name(); \
    static Check::Register name##_register(#name, name); \
    static void name()

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            ++Check::Failures(); \
        } \
    } while (0)

#endif
