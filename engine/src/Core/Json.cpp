#include "Core/Json.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>

namespace Viva {

const Json* Json::Find(std::string_view name) const
{
    const Object* object = AsObject();
    if (!object)
        return nullptr;
    // A linear search: objects in scene files have a handful of members.
    for (const auto& [key, value] : *object) {
        if (key == name)
            return &value;
    }
    return nullptr;
}

double FloatToJsonNumber(float value)
{
    // std::to_chars with no format writes the shortest text that reads back as exactly this
    // float ("0.1"). std::from_chars then reads that text as a double. Both are C++17's fast,
    // locale-independent number conversions (printf would write "0,1" in some languages).
    char text[32];
    const std::to_chars_result written = std::to_chars(text, text + sizeof(text), value);
    double result = value;
    std::from_chars(text, written.ptr, result);
    return result;
}

namespace {

// A recursive descent parser: one function per kind of value, each reading its value and calling
// the others for whatever is inside it, the way the JSON grammar is written. It reads straight
// from the text, keeping a position in it.
//
// The engine doesn't use exceptions, so an error is recorded in m_Error, and every function
// returns false (or an empty optional) from then on, all the way back up.
class Parser {
public:
    explicit Parser(std::string_view text) : m_Text(text) {}

    std::optional<Json> ParseDocument()
    {
        std::optional<Json> value = ParseValue(0);
        if (value) {
            SkipWhitespace();
            if (m_Position != m_Text.size())
                return Fail("unexpected text after the end");
        }
        return value;
    }

    const std::string& GetError() const { return m_Error; }

private:
    // Nesting this deep is no real file: refuse it before the recursion runs out of stack.
    static constexpr int kMaxDepth = 256;

    std::nullopt_t Fail(std::string_view message)
    {
        if (m_Error.empty()) {
            // The line and column of the current position, counting from 1, as editors show them.
            size_t line = 1;
            size_t column = 1;
            for (size_t i = 0; i < m_Position && i < m_Text.size(); ++i) {
                if (m_Text[i] == '\n') {
                    ++line;
                    column = 1;
                } else {
                    ++column;
                }
            }
            m_Error = std::format("line {}, column {}: {}", line, column, message);
        }
        return std::nullopt;
    }

    void SkipWhitespace()
    {
        while (m_Position < m_Text.size()) {
            const char c = m_Text[m_Position];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
                break;
            ++m_Position;
        }
    }

    // The next character, or '\0' at the end of the text.
    char Peek() const { return m_Position < m_Text.size() ? m_Text[m_Position] : '\0'; }

    // Reads `word` if the text continues with it.
    bool Consume(std::string_view word)
    {
        if (m_Text.substr(m_Position, word.size()) != word)
            return false;
        m_Position += word.size();
        return true;
    }

    std::optional<Json> ParseValue(int depth)
    {
        if (depth > kMaxDepth)
            return Fail("nested too deeply");
        SkipWhitespace();
        switch (Peek()) {
        case '{': return ParseObject(depth);
        case '[': return ParseArray(depth);
        case '"': {
            std::optional<std::string> text = ParseString();
            if (!text)
                return std::nullopt;
            return Json(std::move(*text));
        }
        case 't': if (Consume("true")) return Json(true); break;
        case 'f': if (Consume("false")) return Json(false); break;
        case 'n': if (Consume("null")) return Json(); break;
        case '\0': return Fail("unexpected end of the text");
        default: return ParseNumber();
        }
        return Fail("unknown word");
    }

    std::optional<Json> ParseObject(int depth)
    {
        ++m_Position; // {
        Json::Object object;
        SkipWhitespace();
        if (Peek() == '}') {
            ++m_Position;
            return Json(std::move(object));
        }
        while (true) {
            SkipWhitespace();
            if (Peek() != '"')
                return Fail("expected a member name in quotes");
            std::optional<std::string> name = ParseString();
            if (!name)
                return std::nullopt;
            SkipWhitespace();
            if (!Consume(":"))
                return Fail("expected ':' after the member name");
            std::optional<Json> value = ParseValue(depth + 1);
            if (!value)
                return std::nullopt;
            object.emplace_back(std::move(*name), std::move(*value));

            SkipWhitespace();
            if (Consume("}"))
                return Json(std::move(object));
            if (!Consume(","))
                return Fail("expected ',' or '}'");
        }
    }

    std::optional<Json> ParseArray(int depth)
    {
        ++m_Position; // [
        Json::Array array;
        SkipWhitespace();
        if (Peek() == ']') {
            ++m_Position;
            return Json(std::move(array));
        }
        while (true) {
            std::optional<Json> value = ParseValue(depth + 1);
            if (!value)
                return std::nullopt;
            array.push_back(std::move(*value));

            SkipWhitespace();
            if (Consume("]"))
                return Json(std::move(array));
            if (!Consume(","))
                return Fail("expected ',' or ']'");
        }
    }

    std::optional<Json> ParseNumber()
    {
        // from_chars reads as many characters as make a number and says where it stopped. It also
        // takes "inf" and "nan", which JSON doesn't have: a JSON number starts with a digit, or a
        // minus and a digit.
        const size_t digit = Peek() == '-' ? m_Position + 1 : m_Position;
        if (digit >= m_Text.size() || m_Text[digit] < '0' || m_Text[digit] > '9')
            return Fail("expected a value");
        double value = 0.0;
        const char* begin = m_Text.data() + m_Position;
        const char* end = m_Text.data() + m_Text.size();
        const std::from_chars_result read = std::from_chars(begin, end, value);
        if (read.ec != std::errc() || read.ptr == begin)
            return Fail("expected a value");
        m_Position += static_cast<size_t>(read.ptr - begin);
        return Json(value);
    }

    // Appends a Unicode code point as UTF-8: one to four bytes, depending on how big it is.
    static void AppendUtf8(std::string& text, uint32_t codePoint)
    {
        if (codePoint < 0x80) {
            text += static_cast<char>(codePoint);
        } else if (codePoint < 0x800) {
            text += static_cast<char>(0xC0 | (codePoint >> 6));
            text += static_cast<char>(0x80 | (codePoint & 0x3F));
        } else if (codePoint < 0x10000) {
            text += static_cast<char>(0xE0 | (codePoint >> 12));
            text += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            text += static_cast<char>(0x80 | (codePoint & 0x3F));
        } else {
            text += static_cast<char>(0xF0 | (codePoint >> 18));
            text += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
            text += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            text += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
    }

    // The four hex digits after "\u".
    std::optional<uint32_t> ParseHex4()
    {
        uint32_t value = 0;
        const char* begin = m_Text.data() + m_Position;
        if (m_Text.size() - m_Position < 4)
            return Fail("expected four hex digits after \\u");
        const std::from_chars_result read = std::from_chars(begin, begin + 4, value, 16);
        if (read.ec != std::errc() || read.ptr != begin + 4)
            return Fail("expected four hex digits after \\u");
        m_Position += 4;
        return value;
    }

    std::optional<std::string> ParseString()
    {
        ++m_Position; // "
        std::string text;
        while (true) {
            if (m_Position >= m_Text.size())
                return Fail("a string isn't closed");
            const char c = m_Text[m_Position++];
            if (c == '"')
                return text;
            if (static_cast<unsigned char>(c) < 0x20)
                return Fail("a control character inside a string must be escaped");
            if (c != '\\') {
                // The text is UTF-8, and so is std::string: other characters are copied as they
                // are, byte by byte.
                text += c;
                continue;
            }

            // An escape sequence: a backslash and what it stands for.
            switch (m_Position < m_Text.size() ? m_Text[m_Position++] : '\0') {
            case '"': text += '"'; break;
            case '\\': text += '\\'; break;
            case '/': text += '/'; break;
            case 'b': text += '\b'; break;
            case 'f': text += '\f'; break;
            case 'n': text += '\n'; break;
            case 'r': text += '\r'; break;
            case 't': text += '\t'; break;
            case 'u': {
                std::optional<uint32_t> codePoint = ParseHex4();
                if (!codePoint)
                    return std::nullopt;
                // Characters beyond the first 65536 are written as two \u escapes, a "surrogate
                // pair" (that's how UTF-16, which JSON's escapes come from, stores them).
                if (*codePoint >= 0xD800 && *codePoint <= 0xDBFF) {
                    if (!Consume("\\u"))
                        return Fail("a surrogate pair is missing its second half");
                    const std::optional<uint32_t> low = ParseHex4();
                    if (!low)
                        return std::nullopt;
                    if (*low < 0xDC00 || *low > 0xDFFF)
                        return Fail("a surrogate pair's second half is invalid");
                    *codePoint = 0x10000 + ((*codePoint - 0xD800) << 10) + (*low - 0xDC00);
                } else if (*codePoint >= 0xDC00 && *codePoint <= 0xDFFF) {
                    return Fail("a surrogate pair's second half comes first");
                }
                AppendUtf8(text, *codePoint);
                break;
            }
            default: return Fail("unknown escape sequence");
            }
        }
    }

    std::string_view m_Text;
    size_t m_Position = 0;
    std::string m_Error;
};

void WriteString(std::string& out, const std::string& text)
{
    out += '"';
    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            // Other control characters have no short escape. Everything else, UTF-8 included,
            // goes in as it is.
            if (static_cast<unsigned char>(c) < 0x20)
                out += std::format("\\u{:04x}", static_cast<unsigned>(c));
            else
                out += c;
        }
    }
    out += '"';
}

void WriteNumber(std::string& out, double value)
{
    // JSON has no infinity or NaN ("not a number"), so those become null, which reads back as a
    // missing value.
    if (!std::isfinite(value)) {
        out += "null";
        return;
    }
    char text[32];
    const std::to_chars_result written = std::to_chars(text, text + sizeof(text), value);
    out.append(text, written.ptr);
}

void WriteValue(std::string& out, const Json& value, int indent)
{
    const auto newLine = [&out](int spaces) {
        out += '\n';
        out.append(static_cast<size_t>(spaces) * 2, ' ');
    };

    if (value.IsNull()) {
        out += "null";
    } else if (const bool* boolean = value.AsBool()) {
        out += *boolean ? "true" : "false";
    } else if (const double* number = value.AsNumber()) {
        WriteNumber(out, *number);
    } else if (const std::string* text = value.AsString()) {
        WriteString(out, *text);
    } else if (const Json::Array* array = value.AsArray()) {
        bool allNumbers = true;
        for (const Json& element : *array)
            allNumbers = allNumbers && element.IsNumber();
        if (array->empty() || allNumbers) {
            // [1, 2, 3] on one line: vectors and colors read best that way.
            out += '[';
            for (size_t i = 0; i < array->size(); ++i) {
                if (i > 0)
                    out += ", ";
                WriteValue(out, (*array)[i], indent);
            }
            out += ']';
            return;
        }
        out += '[';
        for (size_t i = 0; i < array->size(); ++i) {
            newLine(indent + 1);
            WriteValue(out, (*array)[i], indent + 1);
            if (i + 1 < array->size())
                out += ',';
        }
        newLine(indent);
        out += ']';
    } else if (const Json::Object* object = value.AsObject()) {
        if (object->empty()) {
            out += "{}";
            return;
        }
        out += '{';
        for (size_t i = 0; i < object->size(); ++i) {
            newLine(indent + 1);
            WriteString(out, (*object)[i].first);
            out += ": ";
            WriteValue(out, (*object)[i].second, indent + 1);
            if (i + 1 < object->size())
                out += ',';
        }
        newLine(indent);
        out += '}';
    }
}

} // namespace

std::optional<Json> ParseJson(std::string_view text, std::string& error)
{
    Parser parser(text);
    std::optional<Json> value = parser.ParseDocument();
    if (!value)
        error = parser.GetError();
    return value;
}

std::string WriteJson(const Json& value)
{
    std::string out;
    WriteValue(out, value, 0);
    out += '\n';
    return out;
}

} // namespace Viva
