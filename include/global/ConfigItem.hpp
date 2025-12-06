#pragma once
#include <QString>
#include <QJsonObject>

namespace Configs_ConfigItem{
    class configItem;
}

typedef QMap<QString, std::shared_ptr<Configs_ConfigItem::configItem>> ConfJsMapStat;
typedef ConfJsMapStat & ConfJsMap;
//inline ConfJsMap initConfJsMap() { 
//    return std::make_shared<QMap<QString, std::shared_ptr<Configs_ConfigItem::configItem>>>(); 
//}

namespace Configs_ConfigItem {
    // config 工具
    enum itemType {
        string,
        integer,
        integer64,
        boolean,
        stringList,
        integerList,
        jsonStore,
        jsonStoreList,
    };

    class configItem {
    private:
        size_t ptr;
    public:
        QString name;
        itemType type;

        configItem(QString n, size_t p, itemType t) {
            name = std::move(n);
            ptr = p;
            type = t;
        }
        void * getPtr(void * obj);
        friend class JsonStore;
    };

    class JsonStore {
    public:
        virtual ~JsonStore() = default;
   //     QMap<QString, std::shared_ptr<configItem>> _map;
        void _put(ConfJsMap _map, 
            QString str, void *, itemType type
        );

        

        virtual ConfJsMap _map() = 0;

        std::function<void()> callback_after_load = nullptr;
        std::function<void()> callback_before_save = nullptr;

        QString fn;
        bool load_control_must = false; // must load from file
        bool save_control_compact = false;
        bool save_control_no_save = false;
        QByteArray last_save_content;

        JsonStore() = default;

        explicit JsonStore(QString fileName) {
            fn = std::move(fileName);
        }

        void _setValue(const QString &name, void *p);

   //     void _add(configItem *item);

        QString _name(void *p);

        std::shared_ptr<configItem> _get(const QString &name);

//        void _setValue(const QString &name, void *p);

        QJsonObject ToJson(const QStringList &without = {});

        QByteArray ToJsonBytes();

        virtual void FromJson(QJsonObject object);

        void FromJsonBytes(const QByteArray &data);

        virtual bool Save();

        bool Load();
    };
} // namespace Configs_ConfigItem

using namespace Configs_ConfigItem;