#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Viva {

// A JSON value: null, true/false, a number, a string, an array of values, or an object (named
// values). Scene files are JSON (see Scene::Save), and so will project files be (M14). Like a
// JToken in C#'s Newtonsoft.Json, a Json can hold any of these, and asks say which one it is.
//
// JSON is text that people can read and diff, which is why Unity also writes its scenes as text
// (YAML, a cousin of JSON) by default.
class Json {
public:
    // An array: values in order. An object: named values. Objects keep their members in the order
    // they were added (a std::map would sort them), so a saved file lists "Name" before
    // "Transform" before "Components", the way the code wrote them.
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;

    // Each constructor makes one kind of value, so `Json(1.5)` is a number and `Json(Json::Array{})`
    // an empty array. None is "explicit", so a value converts where a Json is expected:
    //     object.emplace_back("Name", "Floor");
    Json() = default; // null
    Json(bool value) : m_Value(value) {}
    Json(double value) : m_Value(value) {}
    Json(std::string value) : m_Value(std::move(value)) {}
    Json(const char* value) : m_Value(std::string(value)) {}
    Json(Array value) : m_Value(std::move(value)) {}
    Json(Object value) : m_Value(std::move(value)) {}

    // What kind of value this is. std::holds_alternative asks a std::variant (a type-safe union:
    // one value of one of the listed types) which type it holds right now.
    bool IsNull() const { return std::holds_alternative<std::nullptr_t>(m_Value); }
    bool IsBool() const { return std::holds_alternative<bool>(m_Value); }
    bool IsNumber() const { return std::holds_alternative<double>(m_Value); }
    bool IsString() const { return std::holds_alternative<std::string>(m_Value); }
    bool IsArray() const { return std::holds_alternative<Array>(m_Value); }
    bool IsObject() const { return std::holds_alternative<Object>(m_Value); }

    // The value itself, as the kind it is. std::get_if gives a pointer to the variant's value if
    // it holds that type, or nullptr if it doesn't: these return nullptr for the wrong kind.
    const bool* AsBool() const { return std::get_if<bool>(&m_Value); }
    const double* AsNumber() const { return std::get_if<double>(&m_Value); }
    const std::string* AsString() const { return std::get_if<std::string>(&m_Value); }
    const Array* AsArray() const { return std::get_if<Array>(&m_Value); }
    const Object* AsObject() const { return std::get_if<Object>(&m_Value); }
    Array* AsArray() { return std::get_if<Array>(&m_Value); }
    Object* AsObject() { return std::get_if<Object>(&m_Value); }

    // An object's member with this name, or nullptr if there's none (or this isn't an object).
    const Json* Find(std::string_view name) const;

private:
    // std::vector may hold a type that isn't complete yet, which is what lets a Json contain
    // vectors of Json.
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> m_Value = nullptr;
};

// Reads JSON text. Returns std::nullopt if the text isn't valid JSON, with `error` saying where
// and why, for example "line 12, column 5: expected ',' or '}'".
std::optional<Json> ParseJson(std::string_view text, std::string& error);

// Writes a value as JSON text, indented two spaces per level, one member or element per line. An
// array of numbers stays on one line, so a position reads [0, 1.5, 0].
std::string WriteJson(const Json& value);

// The double nearest to the shortest decimal that reads back as `value`. A float holds 0.1 as
// 0.100000001490116..., and as a double that prints with all those digits; this turns it into the
// double that prints as 0.1, which still reads back as exactly the same float.
double FloatToJsonNumber(float value);

} // namespace Viva
