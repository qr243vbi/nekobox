#pragma once

#include <QString>
#include <QJsonObject>
#include <functional>

namespace Configs_ConfigItem{
    struct configItem;
}

typedef QMap<QByteArray, std::shared_ptr<Configs_ConfigItem::configItem>> ConfJsMapStat;
typedef ConfJsMapStat & ConfJsMap;
//inline ConfJsMap initConfJsMap() { 
//    return std::make_shared<QMap<QString, std::shared_ptr<Configs_ConfigItem::configItem>>>(); 
//}

namespace Configs_ConfigItem {
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
        virtual void * getPtr(JsonStore * store) const;
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
        type_double = 11
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

    class JsonStore {
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
        void _put(ConfJsMap _map, const QString& str, QVariantMap *);
        void _put(ConfJsMap _map, const QString& str, QJsonStoreListBase *);
        void _put(ConfJsMap _map, const QString& str, bool **);
        void _put(ConfJsMap _map, const QString& str, double *);
        template<typename T, typename = typename std::enable_if<std::is_base_of<JsonStore, T>::value>::type>
        void _put(ConfJsMap _map, const QString& str, T ** type){
            _put(_map, str, (JsonStore **) type);
        }
        template<typename T, typename = typename std::enable_if<std::is_base_of<JsonStore, T>::value>::type>
        void _put(ConfJsMap _map, const QString& str, std::shared_ptr<T>* type){
            _put(_map, str, (std::shared_ptr<JsonStore>*) type);
        }

        virtual ConfJsMap _map() = 0;

        std::function<void()> callback_after_load = nullptr;
        std::function<void()> callback_before_save = nullptr;

        QString fn;
        bool load_control_must = false; 
        bool save_control_no_save = false;

        JsonStore() = default;

        explicit JsonStore(QString fileName) {
            fn = std::move(fileName);
        }

        void _setValue(const QString &name, const QJsonValue &p);
        void _setValue(JsonStore * store, const void *p);

        QString _name(void *p);

        std::shared_ptr<configItem> _get(const QString &name);

        QJsonObject ToJson(const QStringList &without = {});

        QByteArray ToJsonBytes(const QStringList &without = {});

        void FromJson(QJsonObject object);

        void FromJsonBytes(const QByteArray &data);

        void FromBytes(const QByteArray &data);

        QByteArray ToBytes(const QStringList &without = {});
        
        virtual bool Save();

        virtual bool Load();
        
    };

    std::shared_ptr<configItem> getConfigItem(int i);

} // namespace Configs_ConfigItem

using namespace Configs_ConfigItem;
