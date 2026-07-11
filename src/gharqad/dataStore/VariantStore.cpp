#include <nekobox/dataStore/ConfigItem.hpp>
#include <qjsonobject.h>

void VariantStoreCommon::create(std::shared_ptr<JsonEnum> en, std::shared_ptr<JsonStore> val ){
    create(en->value, val);
}
void VariantStoreCommon::create(const EnumFieldName& en, std::shared_ptr<JsonStore> val) {
    create(this->getEnum()->set(en), val);
};

QJsonObject VariantStoreCommon::ToJson() const{
    QJsonObject obj;
    auto th = (VariantStoreCommon*)this;
    obj["type"] = th->getEnum()->toString();
    obj["store"] = th->getStore()->ToJson();
    return obj;
};

void VariantStoreCommon::FromJson(const QJsonObject & obj){
    this->create(obj["type"].toString());
    auto store = this->getStore();
    if (store != nullptr){
        store->FromJson(obj["store"].toObject());
    }
};

void VariantStoreCommon::ToBytes(QDataStream &data) const{
    size_t index = getIndex();
    data << (unsigned long long)index;
    QByteArray arr = this->getStore()->ToBytes();
    data << arr;
};

void VariantStoreCommon::FromBytes(QDataStream &data){
    unsigned long long index;
    data >> index;
    QByteArray arr;
    data >> arr;   
    this->getStore()->FromBytes(arr);
};

/*
INIT_ENUM(Test)
    ADD_ENUM("one", 1);
    ADD_ENUM("two", 2);
STOP_ENUM

class One: public JsonStore {
public:
    int index = 0;
    NEW_MAP
        ADD_MAP("index_one", index, integer);
    STOP_MAP
};

class Two: public JsonStore {
public:
    int index = 0;
    NEW_MAP
        ADD_MAP("index_two", index, integer);
    STOP_MAP
};

void test(){
    
    VariantStore<TestEnum, One, Two> store;
    store.create<One>();
    store.create<Two>();
}
    */