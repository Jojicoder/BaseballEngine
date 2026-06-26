#include "test_support.h"

#include <cctype>
#include <cstdlib>
#include <string>

namespace joji_tests {
namespace {

class JsonSyntaxChecker {
public:
    explicit JsonSyntaxChecker(const std::string& text) : text_(text) {}

    bool valid() {
        skipWhitespace();
        if (!parseValue()) {
            return false;
        }
        skipWhitespace();
        return pos_ == text_.size();
    }

private:
    bool parseValue() {
        skipWhitespace();
        if (pos_ >= text_.size()) {
            return false;
        }
        const char c = text_[pos_];
        if (c == '"') return parseString();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == 't') return consumeLiteral("true");
        if (c == 'f') return consumeLiteral("false");
        if (c == 'n') return consumeLiteral("null");
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber();
        return false;
    }

    bool parseObject() {
        if (!consume('{')) return false;
        skipWhitespace();
        if (consume('}')) return true;
        while (true) {
            if (!parseString()) return false;
            skipWhitespace();
            if (!consume(':')) return false;
            if (!parseValue()) return false;
            skipWhitespace();
            if (consume('}')) return true;
            if (!consume(',')) return false;
        }
    }

    bool parseArray() {
        if (!consume('[')) return false;
        skipWhitespace();
        if (consume(']')) return true;
        while (true) {
            if (!parseValue()) return false;
            skipWhitespace();
            if (consume(']')) return true;
            if (!consume(',')) return false;
        }
    }

    bool parseString() {
        if (!consume('"')) return false;
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return false;
            if (c == '\\') {
                if (pos_ >= text_.size()) return false;
                const char escaped = text_[pos_++];
                if (escaped == 'u') {
                    for (int i = 0; i < 4; ++i) {
                        if (pos_ >= text_.size() ||
                            !std::isxdigit(static_cast<unsigned char>(text_[pos_++]))) {
                            return false;
                        }
                    }
                } else if (std::string("\"\\/bfnrt").find(escaped) == std::string::npos) {
                    return false;
                }
            }
        }
        return false;
    }

    bool parseNumber() {
        const char* start = text_.c_str() + pos_;
        char* end = nullptr;
        std::strtod(start, &end);
        if (end == start) {
            return false;
        }
        pos_ += static_cast<std::size_t>(end - start);
        return true;
    }

    bool consumeLiteral(const char* literal) {
        const std::string value = literal;
        if (text_.compare(pos_, value.size(), value) != 0) {
            return false;
        }
        pos_ += value.size();
        return true;
    }

    bool consume(char expected) {
        skipWhitespace();
        if (pos_ >= text_.size() || text_[pos_] != expected) {
            return false;
        }
        ++pos_;
        return true;
    }

    void skipWhitespace() {
        while (pos_ < text_.size() &&
               std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    const std::string& text_;
    std::size_t pos_ = 0;
};

} // namespace

bool isValidJson(const std::string& text) {
    JsonSyntaxChecker checker(text);
    return checker.valid();
}

void testJsonSyntaxChecker() {
    CHECK(isValidJson("{}"));
    CHECK(isValidJson("{\"a\":[1,true,null,\"x\"]}"));
    CHECK(!isValidJson("{\"a\":}"));
    CHECK(!isValidJson("[1,]"));
}

} // namespace joji_tests
