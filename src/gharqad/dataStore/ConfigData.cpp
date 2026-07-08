
#include <nekobox/dataStore/Configs.hpp>

namespace Configs {
  namespace Data {
    
    Node Node::undefined(){
      return Node(Tag::Undefined);
    };
    Node Node::null(){
      return Node(Tag::Null);
    };
    Node::Node(const Node& node){
      this->value = node.value;
      this->tag = node.tag;
    }
    Node::Node(Tag tag){
      switch (tag){
        case Tag::String:
        this->value = Value(QString(""));
        break;
        case Tag::Array:
        {
          QList<Node> list = {};
          this->value = Value(list);
        }
        break;
        case Tag::Map:
        {
          QMap<EnumFieldName, Node> map = {};
          this->value = Value(map);
        }
        break;
        case Tag::Null:
        case Tag::Undefined:
        case Tag::True:
        case Tag::False:
        this->value = Value(false);
        break;
        case Tag::Number:
        this->value = Value(static_cast<long double>(0));
        break;
      }
      this->tag = tag;
    }
    Node Node::string(const QString & value){
      Node node(Tag::String);
      node.value = value;
      return node;
    };
    Node Node::boolean(bool value){
      Node node(value ? Tag::True : Tag::False);
      node.value = false;
      return node;
    };
    Node Node::number(long double value){
      Node node(Tag::Number);
      node.value = value;
      return node;
    };
    Node Node::map(){
      return Node(Tag::Map);
    };
    Node Node::array(){
      return Node(Tag::Array);
    };
    bool Node::add( Node val){
      if (!this->isArray()){
        return false;
      }
      auto & list = std::get<QList<Node>>(this->value);
      list.push_back(val);
      return true;
    }
    bool Node::addFirst( Node val){
      if (!this->isArray()){
        return false;
      }
      auto & list = std::get<QList<Node>>(this->value);
      list.push_front(val);
      return true;
    }
    bool Node::addLast( Node val){
      return this->add(val);
    }
    Tag Node::type() const {
      return this->tag;
    };
    bool Node::isNumber() const {
      return this->tag == Tag::Number;
    };
    bool Node::isBoolean() const {
      return this->tag == Tag::True || this->tag == Tag::False;
    };
    bool Node::isMap() const {
      return this->tag == Tag::Map;
    };
    bool Node::isArray() const {
      return this->tag == Tag::Array;
    };
    bool Node::isString() const {
      return this->tag == Tag::String;
    };
    bool Node::isNull() const {
      return this->tag == Tag::Null;
    };
    bool Node::isUndefined() const {
      return this->tag == Tag::Undefined;
    };

    long double Node::toNumber() const {
      if (isNumber()){
        return std::get<long double>(this->value);
      } else if (isString()){
        auto & text = std::get<QString>(this->value);
        if (text.compare("true", Qt::CaseInsensitive)){
          return 1;
        }
        return text.toDouble();
      } else if (isBoolean()){
        return 0 + (this->tag == Tag::True);
      } else if (isArray() || isMap()){
        long double i = 0;
        for (Node node: values()){
          i += node.toNumber();
        }
        return i;
      } else {
        return 0;
      }
    };  
    bool Node::toBoolean() const {
      if (isNumber() || isString() || isBoolean()){
        return this->isNumber() > 0;
      } else if (isArray() || isMap()){
        return count() > 0;
      } else {
        return false;
      }
    };  
   
    QString Node::toString() const {
      if (isNumber()){
        return QString::number((double)std::get<long double>(this->value));
      } else if (isString()){
        return std::get<QString>(this->value);
      } else if (isBoolean()){
        return (this->tag == Tag::True) ? "true" : "false";
      } else {
        return "";
      }  
    };

    long double Node::getNumber(long double def) const {
      if (isNumber()){
        return std::get<long double>(this->value);
      } else {
        return def;
      }
    };
    bool Node::getBoolean(bool def) const{
      if (isBoolean()){
        return this->tag == Tag::True;
      } else {
        return def;
      }
    };
    QString Node::getString(const QString & def ) const {
      if (isString()){
        return std::get<QString>(this->value);
      } else {
        return def;
      }
    };


    size_t Node::count() const {
      if (isArray()){
        return std::get<QList<Node>>(this->value).count();
      } else if (isMap()){
        return std::get<QMap<EnumFieldName, Node>>(this->value).count();
      } else {
        return 0;
      }
    };

    QList<EnumFieldName> Node::keys() const {
      if (!isMap()){
        return {};
      }
      return std::get<QMap<EnumFieldName, Node>>(this->value).keys();
    }

    KeyValueRange<QMap<EnumFieldName, Configs::Data::Node> &> Node::asKeyValueRange() const {
      QMap<EnumFieldName, Configs::Data::Node> map;
      if (isMap()){
        map = std::get<QMap<EnumFieldName, Node>>(this->value);
      } 
      return ::asKeyValueRange(map);
    }

    int Node::toInt() const { return (int)toNumber(); };
    bool Node::toBool() const { return toBoolean(); };
    bool Node::isObject() const { return isMap(); };

    Node::Node(const QString & str){
      *this = Node::string(str);
    };
    Node::Node(bool val){
      *this = Node::boolean(val);
    };
    Node::Node(double val){
      *this = Node::number(val);
    };

    bool Node::isDouble() const{
      return this->isNumber();
    };
    bool Node::isBool() const{
      return this->isBoolean();
    };
    
    QStringList Node::toStringList() const {
      QStringList list;
      for (auto value: this->values()){
        list << value.toString();
      }
      return list;
    };

    QList<int> Node::toIntList() const {
      QList<int> list;
      for (auto value: this->values()){
        list << value.toInt();
      }
      return list;
    };

    Node::Node(const QJsonObject& obj){
      *this = Node::map();
      for (auto [key, value]: ::asKeyValueRange(obj)){
        this->at(key.toString()) = Node::parseJsonValue(value);
      }
    };

    Node::Node(const QJsonArray &obj){
        *this = Node::array();
        for (auto value: obj){
          this->addLast(value);  
        }
    }

    Node::Node(const QJsonValueConstRef & value){
      *this = Node::parseJsonValue(value);
    };

    Node Node::parseJsonValue(const QJsonValueConstRef &value){
        if (value.isArray()){
          return Node(value.toArray());
        } else if (value.isBool()){
          return Node(value.toBool());
        } else if (value.isDouble()){
          return Node(value.toDouble());
        } else if (value.isNull()){
          return Node::null();
        } else if (value.isString()){
          return Node(value.toString());
        } else if (value.isUndefined()){
          none:
          return Node::undefined();
        } else if (value.isObject()){
          return Node(value.toObject());
        } else {
          goto none;
        }
    }

    Node Node::fromVariantMap(const QVariantMap & map){
      return Node(QJsonObject::fromVariantMap(map));
    };


    const Node & Node::at(size_t index) const {
      static Node node;
      if (this->isArray()){
        auto &list = std::get<QList<Node>>(this->value);
        return list.at(index);
      } else {
        node = Node::undefined();
        return node;
      }
    };
    Node & Node::at(size_t index) {
      static Node node;
      if (this->isArray()){
        auto count = this->count();
        while (count <= index){
          this->addLast(Node::null());
        }
        return std::get<QList<Node>>(this->value)[index];
      } else {
        node = Node::undefined();
        return node;
      }
    };
    const Node & Node::at(const EnumFieldName& index) const {
      static Node node;
      if (this->isMap()){
        auto &list = std::get<QMap<EnumFieldName, Node>>(this->value);
        return list.find(index).value();
      } else {
        node = Node::undefined();
        return node;
      }
    };
    Node & Node::at(const EnumFieldName& index) {
      static Node node;
      if (this->isMap()){
        auto &list = std::get<QMap<EnumFieldName, Node>>(this->value);
        auto iter = list.find(index);
        if (iter == list.end()){
          return (list[index] = Node::null());
        } else {
          return iter.value();
        }
      } else {
        node = Node::undefined();
        return node;
      }
    };

    bool Node::isNothing() const {
      return this->isNull() || this->isUndefined();
    }

    const Node & Node::operator [](size_t index) const{
      return this->at(index);
    };
    Node & Node::operator [](size_t index){
      return this->at(index);
    };
    const Node & Node::operator [](const EnumFieldName& index) const{
      return this->at(index);
    };
    Node & Node::operator [](const EnumFieldName& index){
      return this->at(index);
    };

    QVariantList Node::toVariantList() const {
      QVariantList list;
      for (auto val : this->values()){
        list << val.toVariant();
      }
      return list;
    }

    QVariant Node::toVariant() const {
      if (this->isBoolean()){
        return this->toBool();
      } else if (this->isArray()){
        return this->toVariantList();
      } else if (this->isDouble()){
        return (double)this->toNumber();
      } else if (this->isMap()){
        return this->toVariantMap();
      } else if (this->isNothing()){
        variant:
        return QVariant();
      } else if (this->isString()){
        return this->toString();
      } else {
        goto variant;
      }
    }

    QVariantMap Node::toVariantMap() const {
      QVariantMap map;
      for (auto [key, value]: this->asKeyValueRange()){
        map[key.get_name()] = value.toVariant();
      }
      return map;
    }

    bool Node::contains(const EnumFieldName & index) const {
      return this->isMap() && std::get<QMap<EnumFieldName, Node>>(this->value).contains(index);
    };

    QList<Node> Node::values() const {
      if (isMap()){
        return std::get<QMap<EnumFieldName, Node>>(this->value).values();
      } else if (isArray()){
        return std::get<QList<Node>>(this->value);
      } else {
        return {};
      }
    }
  }
}