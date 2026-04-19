#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once
#include "Const.hpp"
#include "Utils.hpp"

namespace Configs {
    QByteArray hash(const QString & str);

    QString FindCoreRealPath();

    extern signed char isAdminCache;

    bool IsAdmin(bool forceRenew=false);

    bool isSetuidSet(const std::string& path);

    QString GetBasePath();
    QString getJsonStoreFileName(short type, long id);
    QString getJsonStorePathName(char type);
} 


#ifndef WIDGET_HPP_INCLUDED
#define WIDGET_HPP_INCLUDED

#include <string>
#include <cstddef> // for std::size_t

struct EnumFieldName {
    // constructors
    EnumFieldName();
    EnumFieldName(QString n);
    EnumFieldName(std::string n);
    EnumFieldName(const char* n);

    // copy / move
    EnumFieldName(EnumFieldName const& other);
    EnumFieldName(EnumFieldName&& other) noexcept;

    // assignment operators
    EnumFieldName& operator=(EnumFieldName const& other); // copy assign
    EnumFieldName& operator=(EnumFieldName&& other) noexcept; // move assign

    // assign from string
    EnumFieldName& operator=(QString const& s);
    EnumFieldName& operator=(QString&& s);

    // assign from std string
    EnumFieldName& operator=(std::string const& s);
    EnumFieldName& operator=(std::string&& s);
    EnumFieldName& operator=(const char * s);

    // set operator: replace stored name (updates lower_name)
    void set_name(QString n);

    // accessors
    const QString& get_name() const noexcept;

    // relational operators
    bool operator<(EnumFieldName const& o) const noexcept;
    bool operator==(EnumFieldName const& o) const noexcept;
    bool operator!=(EnumFieldName const& o) const noexcept;
    bool operator>(EnumFieldName const& o) const noexcept;
    bool operator<=(EnumFieldName const& o) const noexcept;
    bool operator>=(EnumFieldName const& o) const noexcept; 

    // convenience comparison with std::string (case-sensitive on original name)
    bool operator==(const QString& s) const noexcept;
    bool operator!=(const QString& s) const noexcept;

    friend struct EnumFieldNameHasher;
    friend struct EnumFieldNameEqual;

private:
    QString name;
};

// Custom hasher and equality functors (declarations)
struct EnumFieldNameHasher {
    std::size_t operator()(EnumFieldName const& w) const noexcept;
};

struct EnumFieldNameEqual {
    bool operator()(EnumFieldName const& a, EnumFieldName const& b) const noexcept;
};

namespace std {
    template<>
    struct hash<EnumFieldName> {
        std::size_t operator()(EnumFieldName const& w) const noexcept;
    };
}

#endif // WIDGET_HPP_INCLUDED