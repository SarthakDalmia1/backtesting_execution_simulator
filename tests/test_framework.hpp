#pragma once

#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <functional>

// Simple test framework (same style as option pricing engine)
namespace test {

struct TestResult {
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failures;
};

inline TestResult& get_results() {
    static TestResult results;
    return results;
}

inline void reset_results() {
    get_results() = TestResult();
}

#define TEST_CASE(name) \
    void test_##name(); \
    static bool _registered_##name = (test::register_test(#name, test_##name), true); \
    void test_##name()

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            test::get_results().failed++; \
            test::get_results().failures.push_back( \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + " ASSERT_TRUE failed: " #expr); \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(expr) \
    do { \
        if (expr) { \
            test::get_results().failed++; \
            test::get_results().failures.push_back( \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + " ASSERT_FALSE failed: " #expr); \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            test::get_results().failed++; \
            test::get_results().failures.push_back( \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + " ASSERT_EQ failed: " #a " != " #b); \
            return; \
        } \
    } while(0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            test::get_results().failed++; \
            test::get_results().failures.push_back( \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + " ASSERT_NE failed: " #a " == " #b); \
            return; \
        } \
    } while(0)

#define ASSERT_LT(a, b) \
    do { \
        if ((a) >= (b)) { \
            test::get_results().failed++; \
            test::get_results().failures.push_back( \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + " ASSERT_LT failed: " #a " >= " #b); \
            return; \
        } \
    } while(0)

#define ASSERT_LE(a, b) \
    do { \
        if ((a) > (b)) { \
            test::get_results().failed++; \
            test::get_results().failures.push_back( \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + " ASSERT_LE failed: " #a " > " #b); \
            return; \
        } \
    } while(0)

#define ASSERT_GT(a, b) \
    do { \
        if ((a) <= (b)) { \
            test::get_results().failed++; \
            test::get_results().failures.push_back( \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + " ASSERT_GT failed: " #a " <= " #b); \
            return; \
        } \
    } while(0)

#define ASSERT_GE(a, b) \
    do { \
        if ((a) < (b)) { \
            test::get_results().failed++; \
            test::get_results().failures.push_back( \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + " ASSERT_GE failed: " #a " < " #b); \
            return; \
        } \
    } while(0)

#define ASSERT_NEAR(a, b, tol) \
    do { \
        if (std::abs((a) - (b)) > (tol)) { \
            test::get_results().failed++; \
            test::get_results().failures.push_back( \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + " ASSERT_NEAR failed: " + \
                std::to_string(a) + " != " + std::to_string(b) + " (tol=" + std::to_string(tol) + ")"); \
            return; \
        } \
    } while(0)

#define ASSERT_THROWS(expr) \
    do { \
        bool threw = false; \
        try { expr; } catch (...) { threw = true; } \
        if (!threw) { \
            test::get_results().failed++; \
            test::get_results().failures.push_back( \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) + " ASSERT_THROWS failed: " #expr); \
            return; \
        } \
    } while(0)

using TestFunc = std::function<void()>;

inline std::vector<std::pair<std::string, TestFunc>>& get_tests() {
    static std::vector<std::pair<std::string, TestFunc>> tests;
    return tests;
}

inline void register_test(const std::string& name, TestFunc func) {
    get_tests().emplace_back(name, func);
}

inline int run_all_tests() {
    reset_results();
    auto& tests = get_tests();
    
    std::cout << "\nRunning " << tests.size() << " tests...\n\n";
    
    for (const auto& [name, func] : tests) {
        int before_failed = get_results().failed;
        
        try {
            func();
        } catch (const std::exception& e) {
            get_results().failed++;
            get_results().failures.push_back(name + " threw exception: " + e.what());
        }
        
        if (get_results().failed == before_failed) {
            get_results().passed++;
            std::cout << "[PASS] " << name << "\n";
        } else {
            std::cout << "[FAIL] " << name << "\n";
        }
    }
    
    std::cout << "\n========================================\n";
    std::cout << "Results: " << get_results().passed << " passed, "
              << get_results().failed << " failed\n";
    
    if (!get_results().failures.empty()) {
        std::cout << "\nFailures:\n";
        for (const auto& f : get_results().failures) {
            std::cout << "  " << f << "\n";
        }
    }
    
    return get_results().failed > 0 ? 1 : 0;
}

}  // namespace test
