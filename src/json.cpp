#include "json.hpp"

#include <cctype>
#include <cstdio>
#include <stdexcept>

namespace kryptic {
namespace json {
namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    Value parse_value() {
        skip_whitespace();
        if (pos_ >= text_.size()) throw std::runtime_error("unexpected end of JSON");
        const char c = text_[pos_];
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return parse_string();
        if (c == 't') {
            expect("true");
            Value v;
            v.type = Value::Type::Bool;
            v.boolean = true;
            return v;
        }
        if (c == 'f') {
            expect("false");
            Value v;
            v.type = Value::Type::Bool;
            v.boolean = false;
            return v;
        }
        if (c == 'n') {
            expect("null");
            return Value{};
        }
        return parse_number();
    }

private:
    const std::string& text_;
    std::size_t pos_ = 0;

    void skip_whitespace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    char peek() {
        if (pos_ >= text_.size()) throw std::runtime_error("unexpected end of JSON");
        return text_[pos_];
    }

    char next() {
        if (pos_ >= text_.size()) throw std::runtime_error("unexpected end of JSON");
        return text_[pos_++];
    }

    void expect(const char* literal) {
        for (const char* p = literal; *p; ++p) {
            if (next() != *p) throw std::runtime_error(std::string("expected ") + literal);
        }
    }

    Value parse_object() {
        next();  // {
        Value result;
        result.type = Value::Type::Object;
        skip_whitespace();
        if (peek() == '}') {
            next();
            return result;
        }
        while (true) {
            skip_whitespace();
            Value key = parse_string();
            skip_whitespace();
            if (next() != ':') throw std::runtime_error("expected ':'");
            result.object.emplace(key.string, parse_value());
            skip_whitespace();
            const char c = next();
            if (c == '}') return result;
            if (c != ',') throw std::runtime_error("expected ',' or '}'");
        }
    }

    Value parse_array() {
        next();  // [
        Value result;
        result.type = Value::Type::Array;
        skip_whitespace();
        if (peek() == ']') {
            next();
            return result;
        }
        while (true) {
            result.array.push_back(parse_value());
            skip_whitespace();
            const char c = next();
            if (c == ']') return result;
            if (c != ',') throw std::runtime_error("expected ',' or ']'");
        }
    }

    Value parse_string() {
        if (next() != '"') throw std::runtime_error("expected '\"'");
        Value result;
        result.type = Value::Type::String;
        while (true) {
            const char c = next();
            if (c == '"') return result;
            if (c == '\\') {
                const char escaped = next();
                switch (escaped) {
                    case '"':
                    case '\\':
                    case '/':
                        result.string.push_back(escaped);
                        break;
                    case 'n':
                        result.string.push_back('\n');
                        break;
                    case 'r':
                        result.string.push_back('\r');
                        break;
                    case 't':
                        result.string.push_back('\t');
                        break;
                    case 'b':
                        result.string.push_back('\b');
                        break;
                    case 'f':
                        result.string.push_back('\f');
                        break;
                    case 'u': {
                        unsigned code = 0;
                        for (int i = 0; i < 4; ++i) {
                            const char h = next();
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
                            else throw std::runtime_error("bad unicode escape");
                        }
                        if (code < 0x80) {
                            result.string.push_back(static_cast<char>(code));
                        } else if (code < 0x800) {
                            result.string.push_back(static_cast<char>(0xC0 | (code >> 6)));
                            result.string.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        } else {
                            result.string.push_back(static_cast<char>(0xE0 | (code >> 12)));
                            result.string.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                            result.string.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        }
                        break;
                    }
                    default:
                        throw std::runtime_error(std::string("bad escape: \\") + escaped);
                }
            } else {
                result.string.push_back(c);
            }
        }
    }

    Value parse_number() {
        const std::size_t start = pos_;
        while (pos_ < text_.size() && std::string("-+.eE0123456789").find(text_[pos_]) != std::string::npos) {
            ++pos_;
        }
        if (start == pos_) throw std::runtime_error("expected a JSON value");
        Value result;
        result.type = Value::Type::Number;
        result.number = std::stod(text_.substr(start, pos_ - start));
        return result;
    }
};

}  // namespace

const Value* Value::get(const std::string& key) const {
    if (type != Type::Object) return nullptr;
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

std::string Value::as_string() const {
    return type == Type::String ? string : std::string{};
}

bool Value::as_bool() const {
    return type == Type::Bool && boolean;
}

Value parse(const std::string& text) {
    Parser parser(text);
    Value value = parser.parse_value();
    if (value.type != Value::Type::Object) {
        throw std::runtime_error("expected a JSON object");
    }
    return value;
}

std::string quote(const std::string& value) {
    std::string out;
    out.push_back('"');
    for (unsigned char c : value) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
    return out;
}

}  // namespace json
}  // namespace kryptic
