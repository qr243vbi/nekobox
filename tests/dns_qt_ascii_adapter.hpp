// Limited ASCII-only test adapter. It is not Qt and does not test Qt ABI,
// Unicode, allocation behavior, DNS validation or the application integration.
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

class QStringList;
class QString {
public:
  std::string value;
  QString(const char *s = "") : value(s) {}
  QString(std::string s) : value(std::move(s)) {}
  bool startsWith(const char *s) const { return value.starts_with(s); }
  bool contains(const char *s) const { return value.find(s) != value.npos; }
  bool isEmpty() const { return value.empty(); }
  int size() const { return static_cast<int>(value.size()); }
  int count(char16_t c) const { return std::count(value.begin(), value.end(), c); }
  int indexOf(const char *s) const {
    auto n = value.find(s); return n == value.npos ? -1 : static_cast<int>(n);
  }
  int indexOf(char16_t c) const {
    auto n = value.find(static_cast<char>(c));
    return n == value.npos ? -1 : static_cast<int>(n);
  }
  int lastIndexOf(char16_t c) const {
    auto n = value.rfind(static_cast<char>(c));
    return n == value.npos ? -1 : static_cast<int>(n);
  }
  QString left(int n) const { return value.substr(0, n); }
  QString mid(int start, int n = -1) const {
    return value.substr(start, n < 0 ? value.npos : n);
  }
  QString sliced(int start) const { return mid(start); }
  void truncate(int n) { value.resize(n); }
  QString &replace(const char *before, const char *after) {
    const std::string from(before), to(after);
    for (size_t p = 0; (p = value.find(from, p)) != value.npos; p += to.size())
      value.replace(p, from.size(), to);
    return *this;
  }
  int toInt(bool *ok) const {
    char *end = nullptr;
    errno = 0;
    const long n = std::strtol(value.c_str(), &end, 10);
    bool consumed = end != value.c_str();
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    *ok = consumed && !*end && errno != ERANGE && n >= INT_MIN && n <= INT_MAX;
    return *ok ? static_cast<int>(n) : 0;
  }
  bool operator==(const char *s) const { return value == s; }
  auto begin() const { return value.begin(); }
  auto end() const { return value.end(); }
  QStringList split(const char *separator) const;
};
class QStringList : public std::vector<QString> {
public:
  QString last() const { return back(); }
};
inline QStringList QString::split(const char *separator) const {
  QStringList out;
  const std::string sep(separator);
  size_t begin = 0, end;
  while ((end = value.find(sep, begin)) != value.npos) {
    out.emplace_back(value.substr(begin, end - begin));
    begin = end + sep.size();
  }
  out.emplace_back(value.substr(begin));
  return out;
}
class QJsonObject;
class QJsonValue {
public:
  // BuildDnsObject only creates an empty nested object for the tls field.
  std::variant<std::string, int, std::monostate> value;
  QJsonValue() = default;
  QJsonValue(const char *s) : value(std::string(s)) {}
  QJsonValue(const QString &s) : value(s.value) {}
  QJsonValue(int n) : value(n) {}
  QJsonValue(const QJsonObject &);
};
class QJsonObject : public std::map<std::string, QJsonValue> {
public:
  using std::map<std::string, QJsonValue>::map;
};
inline QJsonValue::QJsonValue(const QJsonObject &object) : value(std::monostate{}) {
  if (!object.empty()) throw std::runtime_error("Adapter only supports empty nested objects");
}
inline std::string quote_json(const std::string &s) {
  std::string out = "\"";
  for (const unsigned char c : s) {
    if (c == '\"' || c == '\\') { out += '\\'; out += c; }
    else if (c < 0x20) {
      const char *hex = "0123456789abcdef";
      out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15];
    } else out += c;
  }
  return out + '"';
}
inline std::string dump_json(const QJsonObject &object) {
  std::string out = "{";
  for (const auto &[key, item] : object) {
    if (out.size() != 1) out += ',';
    out += quote_json(key) + ':';
    if (auto s = std::get_if<std::string>(&item.value)) out += quote_json(*s);
    else if (auto n = std::get_if<int>(&item.value)) out += std::to_string(*n);
    else out += "{}";
  }
  return out + '}';
}
