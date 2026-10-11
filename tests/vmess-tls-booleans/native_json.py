"""Additional native-JSON probe with source-linked scalar Node semantics.

Map construction/access uses a declared boundary for independent existing map
recursion; production scalar/array constructors and boolean conversions remain
verbatim, so this probe does not hide the independent main-branch Node bool bug.
"""
from test_link_tls import function


def between(text, start, end):
    if text.count(start) != 1:
        raise ValueError('Source anchor changed: ' + start)
    return text[text.index(start):text.index(end, text.index(start))]


def generate(read):
    header = read('src/nekobox/dataStore/Configs.hpp')
    node = read('src/gharqad/dataStore/ConfigData.cpp')
    enum = read('src/gharqad/dataStore/ConfigItem.cpp')
    code = '''#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMap>
#include <QStringList>
#include <QVariant>
#include <variant>
#include <tuple>
#include <memory>
#include <string>
#include <stdexcept>
'''
    code += between(header, 'struct EnumFieldName {', '// Custom hasher')
    code += '\ntemplate<class T> class KeyValueRange;\nnamespace Configs { namespace Data {\n'
    code += between(header, '        enum class Tag {', '\n    };\n}')
    code += '''
// Explicit map-only boundaries for the independent at(EnumFieldName) recursion.
Node::Node(const QJsonObject &obj) : Node(Tag::Map) {
    auto &map = std::get<QMap<EnumFieldName, Node>>(value);
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        map.insert(EnumFieldName(it.key()), Node(it.value()));
}
const Node &Node::operator[](const EnumFieldName &key) const { return at(QList<EnumFieldName>{key}); }
const Node &Node::operator[](const QList<EnumFieldName> &keys) const { return at(keys); }
const Node &Node::at(const EnumFieldName &key) const { return at(QList<EnumFieldName>{key}); }
QString Node::toQuoted() const { throw std::logic_error("Composite quoting outside VMess probe"); }
QVariantMap Node::toVariantMap() const { throw std::logic_error("Map serialization outside VMess probe"); }
}}
'''
    code += between(enum, 'EnumFieldName::EnumFieldName() = default;', '// --- Hasher and equality functors ---')
    signatures = (
        'Node Node::undefined()', 'Node Node::null()', 'Node::Node(const Node& node)', 'Node::Node(Tag tag)',
        'Node Node::string(', 'Node Node::boolean(', 'Node Node::number(', 'Node Node::map()', 'Node Node::array()',
        'bool Node::add( Node val)', 'bool Node::addLast( Node val)', 'Tag Node::type() const',
        'bool Node::isNumber() const', 'bool Node::isBoolean() const', 'bool Node::isMap() const',
        'bool Node::isArray() const', 'bool Node::isString() const', 'bool Node::isNull() const',
        'bool Node::isUndefined() const', 'bool Node::isDouble() const', 'bool Node::isBool() const',
        'bool Node::isObject() const', 'bool Node::isNothing() const', 'long double Node::toNumber() const',
        'bool Node::toBoolean() const', 'bool Node::toBool() const', 'int Node::toInt() const',
        'bool Node::getBoolean(bool def) const', 'long double Node::getNumber(long double def) const',
        'QString Node::getString(const EnumFieldName & def ) const', 'QString Node::toString() const',
        'QStringList Node::toStringList() const', 'size_t Node::count() const', 'QList<Node> Node::values() const',
        'bool Node::contains(const EnumFieldName & index) const', 'Node::Node(const EnumFieldName & str)',
        'Node::Node(bool val)', 'Node::Node(double val)', 'Node::Node(const QString & value)',
        'Node::Node(const QJsonArray &obj)', 'Node::Node(QJsonValueConstRef value)', 'Node::Node(const QJsonValue & value)',
        'Node Node::parseJsonValue(', 'QVariant Node::toVariant() const', 'QVariantList Node::toVariantList() const',
        'const Node & Node::at(const QList<EnumFieldName>& index) const',
    )
    code += '\nnamespace Configs { namespace Data {\n' + '\n'.join(function(node, s) for s in signatures) + '\n}}\n'
    return code


def imports(read):
    code = '''\nnamespace From_Json {
void add_default_fields(ProxyEntity *, const Data::Node &);
void add_mux_state(AbstractBean *, const Data::Node &);
bool add_tls(std::shared_ptr<V2rayStreamSettings>, const Data::Node &);
bool parse_transport(std::shared_ptr<V2rayStreamSettings>, const Data::Node &);
bool parse_transport(bool, std::shared_ptr<V2rayStreamSettings>, const Data::Node &);
'''
    extra = read('src/nekobox/configs/proxy/AbstractBeanExtra.hpp')
    code += 'template<typename T> ' + function(between(extra, 'namespace From_Json {', 'namespace To_CoreObj_box {'), 'static void add_network(T * t, const Data::Node &obj)') + '\n}\n'
    source = read('src/gharqad/configs/proxy/Json2Bean.cpp')
    for signature in ('void From_Json::add_default_fields(', 'void From_Json::add_mux_state(', 'bool From_Json::add_tls(',
                      'bool From_Json::parse_transport(std::shared_ptr', 'bool From_Json::parse_transport(bool'):
        code += function(source, signature) + '\n'
    code += function(read('src/gharqad/configs/proxy/VMessBean.cpp'), 'bool VMessBean::TryParseJson(') + '\n'
    return code
