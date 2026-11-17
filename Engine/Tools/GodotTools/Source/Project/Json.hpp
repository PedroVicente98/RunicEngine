#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "Result.hpp"

// A small, strict JSON reader and writer for the project's own files (runic.project.json, the IDE
// bridge's files). It reads RFC 8259 JSON: objects, arrays, strings with their escapes (UTF-16
// surrogate pairs included), numbers, true, false and null; it rejects trailing commas, comments
// and duplicate keys. An object keeps its keys in the order they were read or added, so a file
// written back keeps its layout. Private to Runic::GodotTools.

namespace Runic::GodotTools::Detail {

class JsonValue {
  public:
    using Object = std::vector<std::pair<std::string, JsonValue>>;
    using Array = std::vector<JsonValue>;

    JsonValue() = default; // null
    explicit JsonValue(bool value) : m_Value(value) {}
    explicit JsonValue(std::int64_t value) : m_Value(value) {}
    explicit JsonValue(double value) : m_Value(value) {}
    explicit JsonValue(std::string value) : m_Value(std::move(value)) {}
    explicit JsonValue(Object value) : m_Value(std::move(value)) {}
    explicit JsonValue(Array value) : m_Value(std::move(value)) {}

    bool IsNull() const noexcept { return std::holds_alternative<std::monostate>(m_Value); }
    bool IsBool() const noexcept { return std::holds_alternative<bool>(m_Value); }
    bool IsInteger() const noexcept { return std::holds_alternative<std::int64_t>(m_Value); }
    bool IsNumber() const noexcept {
        return IsInteger() || std::holds_alternative<double>(m_Value);
    }
    bool IsString() const noexcept { return std::holds_alternative<std::string>(m_Value); }
    bool IsObject() const noexcept { return std::holds_alternative<Object>(m_Value); }
    bool IsArray() const noexcept { return std::holds_alternative<Array>(m_Value); }

    bool AsBool() const { return std::get<bool>(m_Value); }
    std::int64_t AsInteger() const { return std::get<std::int64_t>(m_Value); }
    double AsNumber() const;
    const std::string &AsString() const { return std::get<std::string>(m_Value); }
    const Object &AsObject() const { return std::get<Object>(m_Value); }
    Object &AsObject() { return std::get<Object>(m_Value); }
    const Array &AsArray() const { return std::get<Array>(m_Value); }
    Array &AsArray() { return std::get<Array>(m_Value); }

    // In an object: the member called key, or nullptr.
    const JsonValue *Find(std::string_view key) const;
    // In an object: sets a member, at the end when it is new.
    void Set(std::string key, JsonValue value);

  private:
    std::variant<std::monostate, bool, std::int64_t, double, std::string, Object, Array> m_Value;
};

// Reads one JSON document. The error names the line and column: "line 3, column 7: ...".
Core::Result<JsonValue> ParseJson(std::string_view text);

// Writes a document with two spaces of indent per level and a final newline.
std::string WriteJson(const JsonValue &value);

} // namespace Runic::GodotTools::Detail
