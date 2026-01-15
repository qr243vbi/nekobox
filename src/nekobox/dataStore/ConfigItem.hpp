#pragma once
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif
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

    struct Bin{
        unsigned short type;
        QByteArray payload;
    };

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
        virtual Bin getBin(JsonStore * store) = 0;
        virtual void setBin(JsonStore * store, const Bin & value) = 0;
        virtual unsigned short type() = 0;
        size_t ptr;
        QString name;
        virtual void * getPtr(JsonStore * store);
    };

    #define PTR_ITEM(X)                            \
    struct X##Item: public configItem {                        \
        QJsonValue getNode(JsonStore * store) override;     \
        void setNode(JsonStore * store, const QJsonValue & value) override; \
        Bin getBin(JsonStore * store) override; \
        void setBin(JsonStore * store, const Bin & value) override; \
        unsigned short type() override { return ConfigItemType::type_##X; } ; \
    };

    enum ConfigItemType{
        type_end,
        type_int, 
        type_long, 
        type_str, 
        type_bool, 
        type_strList, 
        type_intList, 
        type_jsonStore, 
        type_jsonStoreList, 
        type_strMap
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
        void _put(ConfJsMap _map, const QString& str, QVariantMap *);
        void _put(ConfJsMap _map, const QString& str, QJsonStoreListBase *);

        template<typename T = JsonStore>
        void _put(ConfJsMap _map, const QString& str, T ** type){
            _put(_map, str, (JsonStore **) type);
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

        virtual void FromJson(QJsonObject object);

        void FromJsonBytes(const QByteArray &data);
        
        void FromBin(const Bin & value);
        Bin ToBin(const QStringList &without = {});

        virtual bool Save();

        virtual bool Load();
    };

} // namespace Configs_ConfigItem

using namespace Configs_ConfigItem;
