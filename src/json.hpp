#pragma once

#include <map>
#include <string>
#include <vector>

namespace krypticdev {
namespace json {

// Tiny JSON reader so the SDK stays zero-dependency. Supports exactly what
// daemon/PROTOCOL.md and kryptic.json need: objects, arrays, strings, numbers,
// booleans and null.
class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string string;
    std::vector<Value> array;
    std::map<std::string, Value> object;

    const Value* get(const std::string& key) const;
    std::string as_string() const;
    bool as_bool() const;
};

Value parse(const std::string& text);
std::string quote(const std::string& value);

}  // namespace json
}  // namespace krypticdev
