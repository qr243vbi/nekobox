#pragma once

#include <QString>
#include <QJsonObject>
#include <functional>
<<<<<<< HEAD
=======
#include <boost/bimap.hpp>
>>>>>>> other-repo/main

namespace Configs_ConfigItem{
    struct configItem;
}

typedef QMap<QByteArray, std::shared_ptr<Configs_ConfigItem::configItem>> ConfJsMapStat;
typedef ConfJsMapStat & ConfJsMap;
//inline ConfJsMap initConfJsMap() { 
//    return std::make_shared<QMap<QString, std::shared_ptr<Configs_ConfigItem::configItem>>>(); 
//}

namespace Configs_ConfigItem {
<<<<<<< HEAD
    // config
/*
    enum itemType {
        type_string,
        type_integer,
        type_integer64,
        type_boolean,
        type_stringList,
        type_integerList,
        type_jsonStore,
        type_jsonStoreList,
        type_stringMap
    };
*/ 
=======

class JsonEnum{
public:
    virtual const boost::bimap<std::string, int> & _map() const;
    JsonEnum& set(int) ;
    JsonEnum& set(const QString&);
    JsonEnum& set(const char *);
    JsonEnum& set(const QByteArray&);
    JsonEnum& set(const QJsonValue&);
    operator QJsonValue() const;
    operator int() const;
    operator QString() const;
    operator QByteArray() const;

    template<typename K>
    JsonEnum& operator=(K val){
        return this->set(val);
    }
    int value;
};
>>>>>>> other-repo/main

    class JsonStore;

    class QJsonStoreListBase: public QList<JsonStore*> {
        public:
        virtual JsonStore* createJsonStore() = 0;
    };

    template<typename T = JsonStore>
    class QJsonStoreList: public QJsonStoreListBase {
        JsonStore * createJsonStore() override{
            return new T();
        }
    };

    struct configItem {
        virtual QJsonValue getNode(JsonStore * store) = 0;
        virtual void setNode(JsonStore * store, const QJsonValue & value) = 0;
        virtual void serialize(QDataStream & data, JsonStore * store) const = 0;
        virtual void deserialize(QDataStream & data, JsonStore * store) = 0;
        virtual unsigned short type() = 0;
        size_t ptr;
        QString name;
<<<<<<< HEAD
        virtual void * getPtr(JsonStore * store) const;
=======
        virtual void * getPtr(const JsonStore * store) const;
>>>>>>> other-repo/main
    };

    struct Bin{
        configItem * item;
        JsonStore * store;
    };

// Serialization function
inline QDataStream &operator<<(QDataStream &out, const Bin &p) {
    p.item->serialize(out, p.store);
    return out;
}

// Deserialization function
inline QDataStream &operator>>(QDataStream &in, Bin &p) {
    p.item->deserialize(in, p.store);
    return in;
}

    #define PTR_ITEM(X)                                                         \
    struct X##Item: public configItem {                                         \
        QJsonValue getNode(JsonStore * store) override;                         \
        void setNode(JsonStore * store, const QJsonValue & value) override;     \
        void serialize(QDataStream & data, JsonStore * store) const override;   \
        void deserialize(QDataStream & data, JsonStore * store) override;       \
        unsigned short type() override { return ConfigItemType::type_##X; } ;   \
    };

    enum ConfigItemType{
        type_int = 0, 
        type_long = 1, 
        type_str = 2, 
        type_bool = 3, 
        type_strList = 4, 
        type_intList = 5, 
        type_jsonStore = 6, 
        type_jsonStoreList = 7, 
        type_strMap = 8,
        type_boolPtr = 9,
        type_jsonShared = 10,
<<<<<<< HEAD
        type_double = 11
=======
        type_double = 11,
        type_enum = 12
>>>>>>> other-repo/main
    };

    PTR_ITEM(int)
    PTR_ITEM(long)
    PTR_ITEM(str)
    PTR_ITEM(bool)
    PTR_ITEM(strList)
    PTR_ITEM(intList)
    PTR_ITEM(jsonStore)
    PTR_ITEM(jsonStoreList)
    PTR_ITEM(strMap)
    PTR_ITEM(boolPtr)
    PTR_ITEM(jsonShared)
    PTR_ITEM(double)
<<<<<<< HEAD

    class JsonStore {
=======
    PTR_ITEM(enum)

    class JsonStore {
    private:

        std::shared_ptr<configItem> _get_const_job(const QString &name) const;
>>>>>>> other-repo/main
    public:
        virtual ~JsonStore() = default;
   //     QMap<QString, std::shared_ptr<configItem>> _map;
        
    //    void _put<T>(ConfJsMap _map, 
    //        QString str, T * ptr
    //    );
        void _put(ConfJsMap _map, const QString& str, int*);
        void _put(ConfJsMap _map, const QString& str, long long*);
        void _put(ConfJsMap _map, const QString& str, QString*);
        void _put(ConfJsMap _map, const QString& str, bool *);
        void _put(ConfJsMap _map, const QString& str, QStringList *);
        void _put(ConfJsMap _map, const QString& str, QList<int> *);
        void _put(ConfJsMap _map, const QString& str, JsonStore **);
        void _put(ConfJsMap _map, const QString& str, std::shared_ptr<JsonStore>*);
<<<<<<< HEAD
=======
        void _put(ConfJsMap _map, const QString& str, std::shared_ptr<JsonEnum>*);
>>>>>>> other-repo/main
        void _put(ConfJsMap _map, const QString& str, QVariantMap *);
        void _put(ConfJsMap _map, const QString& str, QJsonStoreListBase *);
        void _put(ConfJsMap _map, const QString& str, bool **);
        void _put(ConfJsMap _map, const QString& str, double *);
        template<typename T, typename = typename std::enable_if<std::is_base_of<JsonStore, T>::value>::type>
        void _put(ConfJsMap _map, const QString& str, T ** type){
            _put(_map, str, (JsonStore **) type);
        }
<<<<<<< HEAD
        template<typename T, typename = typename std::enable_if<std::is_base_of<JsonStore, T>::value>::type>
        void _put(ConfJsMap _map, const QString& str, std::shared_ptr<T>* type){
            _put(_map, str, (std::shared_ptr<JsonStore>*) type);
        }

=======

        template<typename T>
        requires (std::derived_from<T, JsonStore> || std::derived_from<T, JsonEnum>)
        void _put(ConfJsMap _map, const QString& str, std::shared_ptr<T> * type){
            using Base = std::conditional_t<
                std::derived_from<T, JsonStore>,
                JsonStore,
                JsonEnum>;
            _put(_map, str, (std::shared_ptr<Base> *) type);
        }
        /*
        template<typename T>
        requires (std::derived_from<T, JsonStore> || std::derived_from<T, JsonEnum>)
        void _put(ConfJsMap map, const QString& str, std::shared_ptr<T>* value)
        {
            using Base = std::conditional_t<
                std::derived_from<T, JsonStore>,
                JsonStore,
                JsonEnum>;

            auto base = std::static_pointer_cast<Base>(*value);
            _put(map, str, &base);
        }
*/
>>>>>>> other-repo/main
        virtual ConfJsMap _map() = 0;

        std::function<void()> callback_after_load = nullptr;
        std::function<void()> callback_before_save = nullptr;

        QString fn;
<<<<<<< HEAD
        bool load_control_must = false; 
=======
 //       bool load_control_must = false; 
>>>>>>> other-repo/main
        bool save_control_no_save = false;

        JsonStore() = default;

        explicit JsonStore(QString fileName) {
            fn = std::move(fileName);
        }

        void _setValue(const QString &name, const QJsonValue &p);
<<<<<<< HEAD
        void _setValue(JsonStore * store, const void *p);
=======
        void _setValue(const JsonStore * store, const void *p);
>>>>>>> other-repo/main

        QString _name(void *p);

        std::shared_ptr<configItem> _get(const QString &name);
<<<<<<< HEAD

        QJsonObject ToJson(const QStringList &without = {});

        QByteArray ToJsonBytes(const QStringList &without = {});
=======
        std::shared_ptr<const configItem> _get_const(const QString &name) const;

        QJsonObject ToJson(const QStringList &without = {}) const;

        QByteArray ToJsonBytes(const QStringList &without = {}) const;
>>>>>>> other-repo/main

        void FromJson(QJsonObject object);

        void FromJsonBytes(const QByteArray &data);

        void FromBytes(const QByteArray &data);

<<<<<<< HEAD
        QByteArray ToBytes(const QStringList &without = {});
=======
        QByteArray ToBytes(const QStringList &without = {}, bool header = false) const;
>>>>>>> other-repo/main
        
        virtual bool Save();

        virtual bool Load();
<<<<<<< HEAD
=======

        virtual bool UnknownKeyHash(const QByteArray &data);
>>>>>>> other-repo/main
        
    };

    std::shared_ptr<configItem> getConfigItem(int i);

} // namespace Configs_ConfigItem

using namespace Configs_ConfigItem;
