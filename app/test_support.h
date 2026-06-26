#pragma once

#include <iostream>
#include <string>

namespace joji_tests {

inline int failures = 0;

inline void check(bool condition, const char* expression, const char* file, int line) {
    if (condition) {
        return;
    }
    ++failures;
    std::cerr << file << ":" << line << ": check failed: " << expression << "\n";
}

bool isValidJson(const std::string& text);
void testJsonSyntaxChecker();

} // namespace joji_tests

#define CHECK(expr) joji_tests::check((expr), #expr, __FILE__, __LINE__)
