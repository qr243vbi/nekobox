



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
            Boolean,
            Map,
            Array,
            String,
            Number,
            None
        };
        class Node;
        union Value {
            bool flag;
            QMap<EnumFieldName, Node> map;
            QList<Node> array;
            QString text;
            long double number; 
            Value(Tag tag);
        };
        class Node {
        private:
            Node(Tag tag);
            Tag tag;
            std::shared_ptr<Value> value;
        public:
            static Node none();
            static Node string(const QString & value);
            static Node boolean(bool value);
            static Node number(long double value);
            static Node map();
            static Node array();
            Tag type() const;
            bool isNumber() const;
            bool isBoolean() const;
            bool isMap() const;
            bool isArray() const;
            bool isString() const;
            bool isNone() const;

            long double toNumber() const;  
            bool toBoolean() const;
            QString toString() const;

            long double getNumber(long double def = 0) const;
            bool getBoolean(bool def = 0) const;
            QString getString(const QString & def = "") const;

            Node get(size_t index, const Node & def = Node::none());
            const Node get(size_t index, const Node & def = Node::none()) const;
            
            Node set(size_t index, const Node & value);

            Node get(const QString &index, const Node & def = Node::none());
            const Node get(const QString &index, const Node & def = Node::none()) const;
            
            Node set(const QString &index, const Node & value);
            size_t count() const;

            KeyValueRange<QMap<EnumFieldName, Configs::Data::Node> &> asKeyValueRange() const;

            QList<EnumFieldName> keys() const;
            QList<Node> values() const;
        };
    };
}

#endif // WIDGET_HPP_INCLUDED