



#pragma once
#include "Const.hpp"
#include "Utils.hpp"
#include <memory>

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


template<typename B, typename C>
inline QMap<EnumFieldName, QString> QStdMapString2QMapEnumFieldName(const std::map<B, B, C> &mp){
    QMap<EnumFieldName, QString> vm;

    for (auto it = mp.begin(); it != mp.end(); ++it) {
        vm.insert(QString::fromStdString(it->first), QString::fromStdString(it->second));
    }
    return vm;
}

namespace Configs {
    namespace Data {

        enum class Tag {
            True,
            False,
            Map,
            Array,
            String,
            Number,
            Null,
            Undefined
        };
        class Node;
        using Value = std::variant<
            QMap<EnumFieldName, Node>,
            QList<Node>,
            QString,
            long double,
            bool
        >;
        class Node {
        private:
            Tag tag;
            Value value;
        public:
            Node(Tag tag = Tag::Undefined);
            Node(const Node& node);
            static Node null();
            static Node undefined();
            static Node string(const EnumFieldName & value);
            static Node boolean(bool value);
            static Node number(long double value);
            static Node map();
            static Node array();
            Tag type() const;
            bool contains(const EnumFieldName &) const;
            bool isNumber() const;
            bool isBoolean() const;
            bool isMap() const;
            bool isArray() const;
            bool isString() const;

            bool isObject() const;
            bool isNull() const;
            bool isNothing() const;
            bool isUndefined() const;
            bool isDouble() const;
            bool isBool() const;
            int toInt() const;
            bool toBool() const;
            long double toNumber() const;  
            bool toBoolean() const;
            QString toString() const;
            QStringList toStringList() const;
            QList<int> toIntList() const;

            long double getNumber(long double def = 0) const;
            bool getBoolean(bool def = 0) const;
            QString getString(const EnumFieldName & def = "") const;

            bool add( Node  value);
            bool addFirst( Node  value);
            bool addLast( Node  value);
            Node(const QJsonValueConstRef & value);

            size_t count() const;

            KeyValueRange<QMap<EnumFieldName, Configs::Data::Node> &> asKeyValueRange() const;

            QList<EnumFieldName> keys() const;
            QList<Node> values() const;

            Node(const EnumFieldName &);
            Node(bool);
            Node(double);
            Node(const QJsonObject&);
            Node(const QJsonArray&);

            const Node & operator [](size_t) const;
            Node & operator [](size_t);

            const Node & operator [](const EnumFieldName&) const;
            Node & operator [](const EnumFieldName&);

            const Node & operator [](const QList<EnumFieldName>&) const;
            Node & operator [](const QList<EnumFieldName>&);

            const Node & at(size_t) const;
            Node & at(size_t);

            const Node & at(const EnumFieldName&) const;
            Node & at(const EnumFieldName&);

            const Node & at(const QList<EnumFieldName>&) const;
            Node & at(const QList<EnumFieldName>&);

            static Node parseJsonValue(const QJsonValueConstRef & value);
            static Node fromVariantMap(const QVariantMap & map);
            QVariantMap toVariantMap() const;
            QVariant toVariant() const;
            QVariantList toVariantList() const;
        };
    };
}

#endif // WIDGET_HPP_INCLUDED