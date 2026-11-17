// The JSON reader and writer (Json.hpp): a recursive-descent parser that tracks line and column for
// its errors, and a writer that indents.

#include "Project/Json.hpp"

#include <charconv>
#include <cmath>
#include <format>
#include <limits>

namespace Runic::GodotTools::Detail {

namespace {

constexpr std::size_t kMaxDepth = 64; // nesting deeper than this is refused, not recursed into
constexpr int kIndent = 2;
constexpr char kNewline = '\n';
constexpr int kHexDigits = 4;
constexpr int kHexBase = 16;
constexpr std::uint32_t kHighSurrogateFirst = 0xD800;
constexpr std::uint32_t kLowSurrogateFirst = 0xDC00;
constexpr std::uint32_t kLowSurrogateLast = 0xDFFF;
constexpr std::uint32_t kSurrogateBits = 10;
constexpr std::uint32_t kSupplementaryFirst = 0x10000;
constexpr std::uint32_t kOneByteLimit = 0x80;
constexpr std::uint32_t kTwoByteLimit = 0x800;
constexpr std::uint32_t kThreeByteLimit = 0x10000;
constexpr std::uint32_t kControlLimit = 0x20;
constexpr std::string_view kTrue = "true";
constexpr std::string_view kFalse = "false";
constexpr std::string_view kNull = "null";

// UTF-8: a lead byte that says how many bytes follow, then six bits per continuation byte.
constexpr std::uint32_t kTwoByteLead = 0xC0;
constexpr std::uint32_t kThreeByteLead = 0xE0;
constexpr std::uint32_t kFourByteLead = 0xF0;
constexpr std::uint32_t kContinuation = 0x80;
constexpr std::uint32_t kContinuationBits = 6;
constexpr std::uint32_t kContinuationMask = 0x3F;

char Continuation(std::uint32_t code, std::uint32_t shift) {
    return static_cast<char>(kContinuation | ((code >> shift) & kContinuationMask));
}

void AppendUtf8(std::string &out, std::uint32_t code) {
    constexpr std::uint32_t kOne = kContinuationBits;
    constexpr std::uint32_t kTwo = kContinuationBits * 2;
    constexpr std::uint32_t kThree = kContinuationBits * 3;
    if (code < kOneByteLimit) {
        out += static_cast<char>(code);
    } else if (code < kTwoByteLimit) {
        out += static_cast<char>(kTwoByteLead | (code >> kOne));
        out += Continuation(code, 0);
    } else if (code < kThreeByteLimit) {
        out += static_cast<char>(kThreeByteLead | (code >> kTwo));
        out += Continuation(code, kOne);
        out += Continuation(code, 0);
    } else {
        out += static_cast<char>(kFourByteLead | (code >> kThree));
        out += Continuation(code, kTwo);
        out += Continuation(code, kOne);
        out += Continuation(code, 0);
    }
}

class Parser {
  public:
    explicit Parser(std::string_view text) : m_Text(text) {}

    Core::Result<JsonValue> Document() {
        JsonValue value;
        if (!Value(value, 0)) {
            return Core::Fail(m_Error);
        }
        SkipBlanks();
        if (m_At != m_Text.size()) {
            return Core::Fail(Where() + "text after the document");
        }
        return value;
    }

  private:
    std::string Where() const {
        int line = 1;
        int column = 1;
        for (std::size_t i = 0; i < m_At && i < m_Text.size(); ++i) {
            if (m_Text[i] == kNewline) {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        return std::format("line {}, column {}: ", line, column);
    }

    bool Fail(std::string_view what) {
        if (m_Error.empty()) {
            m_Error = Where() + std::string(what);
        }
        return false;
    }

    void SkipBlanks() {
        while (m_At < m_Text.size() && (m_Text[m_At] == ' ' || m_Text[m_At] == '\t' ||
                                        m_Text[m_At] == '\n' || m_Text[m_At] == '\r')) {
            ++m_At;
        }
    }

    bool Literal(std::string_view word) {
        if (m_Text.substr(m_At, word.size()) != word) {
            return Fail("not a JSON value");
        }
        m_At += word.size();
        return true;
    }

    bool Value(JsonValue &out, std::size_t depth) {
        if (depth > kMaxDepth) {
            return Fail("nested too deeply");
        }
        SkipBlanks();
        if (m_At >= m_Text.size()) {
            return Fail("the document ends where a value should be");
        }
        switch (m_Text[m_At]) {
        case '{':
            return ObjectValue(out, depth);
        case '[':
            return ArrayValue(out, depth);
        case '"': {
            std::string text;
            if (!String(text)) {
                return false;
            }
            out = JsonValue(std::move(text));
            return true;
        }
        case 't':
            out = JsonValue(true);
            return Literal(kTrue);
        case 'f':
            out = JsonValue(false);
            return Literal(kFalse);
        case 'n':
            out = JsonValue();
            return Literal(kNull);
        default:
            return Number(out);
        }
    }

    bool ObjectValue(JsonValue &out, std::size_t depth) {
        ++m_At; // {
        JsonValue::Object members;
        SkipBlanks();
        if (m_At < m_Text.size() && m_Text[m_At] == '}') {
            ++m_At;
            out = JsonValue(std::move(members));
            return true;
        }
        while (true) {
            SkipBlanks();
            if (m_At >= m_Text.size() || m_Text[m_At] != '"') {
                return Fail("expected a member name in quotes");
            }
            std::string key;
            if (!String(key)) {
                return false;
            }
            for (const auto &member : members) {
                if (member.first == key) {
                    return Fail(std::format("the member \"{}\" appears twice", key));
                }
            }
            SkipBlanks();
            if (m_At >= m_Text.size() || m_Text[m_At] != ':') {
                return Fail("expected ':' after a member name");
            }
            ++m_At;
            JsonValue value;
            if (!Value(value, depth + 1)) {
                return false;
            }
            members.emplace_back(std::move(key), std::move(value));
            SkipBlanks();
            if (m_At < m_Text.size() && m_Text[m_At] == ',') {
                ++m_At;
                continue;
            }
            if (m_At < m_Text.size() && m_Text[m_At] == '}') {
                ++m_At;
                out = JsonValue(std::move(members));
                return true;
            }
            return Fail("expected ',' or '}' in an object");
        }
    }

    bool ArrayValue(JsonValue &out, std::size_t depth) {
        ++m_At; // [
        JsonValue::Array items;
        SkipBlanks();
        if (m_At < m_Text.size() && m_Text[m_At] == ']') {
            ++m_At;
            out = JsonValue(std::move(items));
            return true;
        }
        while (true) {
            JsonValue value;
            if (!Value(value, depth + 1)) {
                return false;
            }
            items.push_back(std::move(value));
            SkipBlanks();
            if (m_At < m_Text.size() && m_Text[m_At] == ',') {
                ++m_At;
                continue;
            }
            if (m_At < m_Text.size() && m_Text[m_At] == ']') {
                ++m_At;
                out = JsonValue(std::move(items));
                return true;
            }
            return Fail("expected ',' or ']' in an array");
        }
    }

    bool Hex4(std::uint32_t &code) {
        if (m_At + kHexDigits > m_Text.size()) {
            return Fail("a \\u escape needs four hex digits");
        }
        const char *first = m_Text.data() + m_At;
        const auto [end, error] = std::from_chars(first, first + kHexDigits, code, kHexBase);
        if (error != std::errc() || end != first + kHexDigits) {
            return Fail("a \\u escape needs four hex digits");
        }
        m_At += kHexDigits;
        return true;
    }

    bool String(std::string &out) {
        ++m_At; // "
        while (m_At < m_Text.size()) {
            const char c = m_Text[m_At++];
            if (c == '"') {
                return true;
            }
            if (static_cast<unsigned char>(c) < kControlLimit) {
                return Fail("a control character inside a string");
            }
            if (c != '\\') {
                out += c;
                continue;
            }
            if (m_At >= m_Text.size()) {
                break;
            }
            const char escape = m_Text[m_At++];
            switch (escape) {
            case '"':
            case '\\':
            case '/':
                out += escape;
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case 'u': {
                std::uint32_t code = 0;
                if (!Hex4(code)) {
                    return false;
                }
                if (code >= kHighSurrogateFirst && code < kLowSurrogateFirst) {
                    std::uint32_t low = 0;
                    if (m_Text.substr(m_At, 2) != "\\u") {
                        return Fail("a high surrogate without its low surrogate");
                    }
                    m_At += 2;
                    if (!Hex4(low) || low < kLowSurrogateFirst || low > kLowSurrogateLast) {
                        return Fail("a high surrogate without its low surrogate");
                    }
                    code = kSupplementaryFirst + ((code - kHighSurrogateFirst) << kSurrogateBits) +
                           (low - kLowSurrogateFirst);
                } else if (code >= kLowSurrogateFirst && code <= kLowSurrogateLast) {
                    return Fail("a low surrogate on its own");
                }
                AppendUtf8(out, code);
                break;
            }
            default:
                return Fail("an unknown escape in a string");
            }
        }
        return Fail("a string is not closed");
    }

    bool Number(JsonValue &out) {
        const std::size_t start = m_At;
        if (m_At < m_Text.size() && m_Text[m_At] == '-') {
            ++m_At;
        }
        const auto digits = [&] {
            const std::size_t from = m_At;
            while (m_At < m_Text.size() && m_Text[m_At] >= '0' && m_Text[m_At] <= '9') {
                ++m_At;
            }
            return m_At - from;
        };
        const std::size_t whole = m_At;
        if (digits() == 0) {
            return Fail("not a JSON value");
        }
        if (m_Text[whole] == '0' && m_At - whole > 1) {
            return Fail("a number with a leading zero");
        }
        bool integer = true;
        if (m_At < m_Text.size() && m_Text[m_At] == '.') {
            ++m_At;
            integer = false;
            if (digits() == 0) {
                return Fail("a number needs digits after its point");
            }
        }
        if (m_At < m_Text.size() && (m_Text[m_At] == 'e' || m_Text[m_At] == 'E')) {
            ++m_At;
            integer = false;
            if (m_At < m_Text.size() && (m_Text[m_At] == '+' || m_Text[m_At] == '-')) {
                ++m_At;
            }
            if (digits() == 0) {
                return Fail("a number needs digits in its exponent");
            }
        }
        const char *first = m_Text.data() + start;
        const char *last = m_Text.data() + m_At;
        if (integer) {
            std::int64_t value = 0;
            const auto [end, error] = std::from_chars(first, last, value);
            if (error == std::errc() && end == last) {
                out = JsonValue(value);
                return true;
            }
        }
        double value = 0.0;
        const auto [end, error] = std::from_chars(first, last, value);
        if (error != std::errc() || end != last || !std::isfinite(value)) {
            return Fail("a number out of range");
        }
        out = JsonValue(value);
        return true;
    }

    std::string_view m_Text;
    std::size_t m_At = 0;
    std::string m_Error;
};

void WriteString(std::string &out, const std::string &text) {
    out += '"';
    for (const char c : text) {
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
            if (static_cast<unsigned char>(c) < kControlLimit) {
                out += std::format("\\u{:04x}", static_cast<unsigned>(c));
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

void Write(std::string &out, const JsonValue &value, int depth) {
    const std::string inner(static_cast<std::size_t>((depth + 1) * kIndent), ' ');
    const std::string outer(static_cast<std::size_t>(depth * kIndent), ' ');
    if (value.IsNull()) {
        out += kNull;
    } else if (value.IsBool()) {
        out += value.AsBool() ? kTrue : kFalse;
    } else if (value.IsInteger()) {
        out += std::to_string(value.AsInteger());
    } else if (value.IsNumber()) {
        out += std::format("{}", value.AsNumber());
    } else if (value.IsString()) {
        WriteString(out, value.AsString());
    } else if (value.IsArray()) {
        const auto &items = value.AsArray();
        if (items.empty()) {
            out += "[]";
            return;
        }
        out += "[\n";
        for (std::size_t i = 0; i < items.size(); ++i) {
            out += inner;
            Write(out, items[i], depth + 1);
            out += i + 1 < items.size() ? ",\n" : "\n";
        }
        out += outer + "]";
    } else {
        const auto &members = value.AsObject();
        if (members.empty()) {
            out += "{}";
            return;
        }
        out += "{\n";
        for (std::size_t i = 0; i < members.size(); ++i) {
            out += inner;
            WriteString(out, members[i].first);
            out += ": ";
            Write(out, members[i].second, depth + 1);
            out += i + 1 < members.size() ? ",\n" : "\n";
        }
        out += outer + "}";
    }
}

} // namespace

double JsonValue::AsNumber() const {
    return IsInteger() ? static_cast<double>(AsInteger()) : std::get<double>(m_Value);
}

const JsonValue *JsonValue::Find(std::string_view key) const {
    for (const auto &member : AsObject()) {
        if (member.first == key) {
            return &member.second;
        }
    }
    return nullptr;
}

void JsonValue::Set(std::string key, JsonValue value) {
    for (auto &member : AsObject()) {
        if (member.first == key) {
            member.second = std::move(value);
            return;
        }
    }
    AsObject().emplace_back(std::move(key), std::move(value));
}

Core::Result<JsonValue> ParseJson(std::string_view text) { return Parser(text).Document(); }

std::string WriteJson(const JsonValue &value) {
    std::string out;
    Write(out, value, 0);
    out += kNewline;
    return out;
}

} // namespace Runic::GodotTools::Detail
