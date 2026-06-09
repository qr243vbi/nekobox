#pragma once

#include <QString>
#include <QMap>
#include <QList>
#include <QTextStream>
#include <QSharedPointer>
#include <QVariant>
#include <type_traits>

namespace iniqt {

namespace detail {

// Safely replaces instances of 'from' with 'to' in a string using Qt
inline bool replace(QString &str, const QString &from, const QString &to) {
    if (!str.contains(from)) {
        return false;
    }
    str.replace(from, to);
    return true;
}

} // namespace detail

// Converts a QString to a target type T using QVariant casting abstractions
template <typename T>
inline bool extract(const QString &value, T &dst) {
    QVariant var(value);
    if constexpr (std::is_same_v<T, bool>) {
        // Handle special JavaScript/INI string booleans safely
        QString lower = value.toLower().trimmed();
        if (lower == "true" || lower == "1" || lower == "yes") {
            dst = true;
            return true;
        }
        if (lower == "false" || lower == "0" || lower == "no") {
            dst = false;
            return true;
        }
        return false;
    } else {
        if (var.canConvert<T>()) {
            dst = var.value<T>();
            return true;
        }
        return false;
    }
}

// Specialization if the target type is already a QString
inline bool extract(const QString &value, QString &dst) {
    dst = value;
    return true;
}

// Automatically serializes any type T into a string representation inside the Section map
template <typename T>
inline void set_value(QMap<QString, QString> &sec, const QString &key, const T &src) {
    if constexpr (std::is_same_v<T, bool>) {
        // Enforce clean explicit string primitives for booleans ("true"/"false")
        sec.insert(key, src ? "true" : "false");
    } else if constexpr (std::is_same_v<T, QString>) {
        sec.insert(key, src);
    } else {
        // Fall back to QVariant's structural string generation path
        sec.insert(key, QVariant::fromValue(src).toString());
    }
}

// Overload helper to accept raw literal C-strings (e.g., set_value(sec, "key", "my_string_literal"))
inline void set_value(QMap<QString, QString> &sec, const QString &key, const char* src) {
    sec.insert(key, QString::fromUtf8(src));
}

// Extracts value from map and safely converts it to type T
template <typename T>
inline bool get_value(const QMap<QString, QString> &sec, const QString &key, T &dst) {
    auto it = sec.find(key);
    if (it == sec.end()) return false;
    return extract(it.value(), dst);
}

class Format
{
public:
    const QChar char_section_start;
    const QChar char_section_end;
    const QChar char_assign;
    const QChar char_comment;

    virtual bool is_section_start(QChar ch) const { return ch == char_section_start; }
    virtual bool is_section_end(QChar ch) const { return ch == char_section_end; }
    virtual bool is_assign(QChar ch) const { return ch == char_assign; }
    virtual bool is_comment(QChar ch) const { return ch == char_comment; }

    const QChar char_interpol;
    const QChar char_interpol_start;
    const QChar char_interpol_sep;
    const QChar char_interpol_end;

    Format(QChar section_start, QChar section_end, QChar assign, QChar comment, 
           QChar interpol, QChar interpol_start, QChar interpol_sep, QChar interpol_end)
        : char_section_start(section_start)
        , char_section_end(section_end)
        , char_assign(assign)
        , char_comment(comment)
        , char_interpol(interpol)
        , char_interpol_start(interpol_start)
        , char_interpol_sep(interpol_sep)
        , char_interpol_end(interpol_end) {}

    Format() : Format('[', ']', '=', ';', '$', '{', ':', '}') {}

    const QString local_symbol(const QString& name) const {
        return QString(char_interpol) + char_interpol_start + name + char_interpol_end;
    }

    const QString global_symbol(const QString& sec_name, const QString& name) const {
        return local_symbol(sec_name + char_interpol_sep + name);
    }
};

class Ini
{
public:
    using Section = QMap<QString, QString>;
    using Sections = QMap<QString, Section>;

    Sections sections;
    QList<QString> errors;
    QSharedPointer<Format> format;

    static const int max_interpolation_depth = 10;

    Ini() : format(QSharedPointer<Format>::create()) {}
    Ini(QSharedPointer<Format> fmt) : format(fmt) {}

    // Generates an INI file syntax output to a QTextStream
    void generate(QTextStream &os) const {
        for (auto it = sections.cbegin(); it != sections.cend(); ++it) {
            os << format->char_section_start << it.key() << format->char_section_end << "\n";
            const auto &sec = it.value();
            for (auto vIt = sec.cbegin(); vIt != sec.cend(); ++vIt) {
                os << vIt.key() << format->char_assign << vIt.value() << "\n";
            }
            os << "\n";
        }
    }

    // Parses raw INI configuration lines from a QTextStream source pipeline
    void parse(QTextStream &is) {
        QString line;
        QString section;
        while (is.readLineInto(&line)) {
            line = line.trimmed();
            const auto length = line.length();
            if (length > 0) {
                int assign_idx = line.indexOf(format->char_assign);
                const QChar front = line.front();

                if (format->is_comment(front)) {
                    // Ignore line comment lines
                }
                else if (format->is_section_start(front)) {
                    if (format->is_section_end(line.back()))
                        section = line.mid(1, length - 2);
                    else
                        errors.append(line);
                }
                else if (assign_idx > 0) {
                    QString variable = line.left(assign_idx).trimmed();
                    QString value = line.mid(assign_idx + 1).trimmed();
                    
                    auto &sec = sections[section];
                    if (!sec.contains(variable))
                        sec.insert(variable, value);
                    else
                        errors.append(line);
                }
                else {
                    errors.append(line);
                }
            }
        }
    }

    void interpolate() {
        int global_iteration = 0;
        auto changed = false;
        
        for (auto it = sections.begin(); it != sections.end(); ++it) {
            replace_symbols(local_symbols(it.key(), it.value()), it.value());
        }
        
        do {
            changed = false;
            const auto syms = global_symbols();
            for (auto &sec : sections) {
                changed |= replace_symbols(syms, sec);
            }
        } while (changed && (max_interpolation_depth > global_iteration++));
    }

    void default_section(const Section &sec) {
        for (auto &sec2 : sections) {
            for (auto it = sec.cbegin(); it != sec.cend(); ++it) {
                if (!sec2.contains(it.key())) {
                    sec2.insert(it.key(), it.value());
                }
            }
        }
    }

    void strip_trailing_comments() {
        for (auto &sec : sections) {
            for (auto &value : sec) {
                int comment_idx = value.indexOf(format->char_comment);
                if (comment_idx >= 0) {
                    value = value.left(comment_idx).trimmed();
                }
            }
        }
    }

    void clear() {
        sections.clear();
        errors.clear();
    }

private:
    struct SymbolPair {
        QString first;
        QString second;
    };
    using Symbols = QList<SymbolPair>;

    const Symbols local_symbols(const QString &sec_name, const Section &sec) const {
        Symbols result;
        for (auto it = sec.cbegin(); it != sec.cend(); ++it) {
            result.append({format->local_symbol(it.key()), format->global_symbol(sec_name, it.key())});
        }
        return result;
    }

    const Symbols global_symbols() const {
        Symbols result;
        for (auto it = sections.cbegin(); it != sections.cend(); ++it) {
            const auto &sec = it.value();
            for (auto vIt = sec.cbegin(); vIt != sec.cend(); ++vIt) {
                result.append({format->global_symbol(it.key(), vIt.key()), vIt.value()});
            }
        }
        return result;
    }

    bool replace_symbols(const Symbols &syms, Section &sec) const {
        auto changed = false;
        for (const auto &sym : syms) {
            for (auto &value : sec) {
                changed |= detail::replace(value, sym.first, sym.second);
            }
        }
        return changed;
    }
};

} // namespace inipp
