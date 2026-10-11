using Configs::Data::Node;
using Configs::Data::Tag;
static int checks = 0;
static int failures = 0;
static void check(bool condition, const char *description) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

static_assert(std::is_same_v<decltype(std::declval<Node&>().at(std::declval<const EnumFieldName&>())), Node&>);
static_assert(std::is_same_v<decltype(std::declval<const Node&>().at(std::declval<const EnumFieldName&>())), const Node&>);
static_assert(std::is_same_v<decltype(std::declval<Node&>()[std::declval<const EnumFieldName&>()]), Node&>);
static_assert(std::is_same_v<decltype(std::declval<const Node&>()[std::declval<const EnumFieldName&>()]), const Node&>);

static void test_key_overloads() {
    const EnumFieldName key("Tls-Enabled");
    const QList<EnumFieldName> path{key};
    Node map = Node::map();
    map.at(path) = Node::number(7);
    const Node &view = map;
    check(map.at(key).getNumber() == 7, "mutable named at reads existing key");
    check(view.at(key).getNumber() == 7, "const named at reads existing key");
    check(map[key].getNumber() == 7, "mutable named operator[] reads existing key");
    check(view[key].getNumber() == 7, "const named operator[] reads existing key");
    check(&map.at(key) == &map.at(path), "mutable named at returns the stored list-overload reference");
    check(&view.at(key) == &view.at(path), "const named at returns the stored list-overload reference");
    check(&map[key] == &map[path], "mutable named operator[] preserves reference identity");
    check(&view[key] == &view[path], "const named operator[] preserves reference identity");
    check(map.at(QString("tls_enabled")).getNumber() == 7, "QString key preserves production normalization");
    check(view.at("TLSENABLED").getNumber() == 7, "const C-string key preserves production normalization");
    check(map[EnumFieldName("TLS_enabled")].getNumber() == 7, "operator[] preserves production normalization");
    map.at(key) = Node::number(8);
    check(view.at(path).getNumber() == 8, "mutable named at updates backing map");
    map[key] = Node::number(9);
    check(view[path].getNumber() == 9, "mutable named operator[] updates backing map");
    check(map.count() == 1, "equivalent key reads and writes do not duplicate entries");
    check(view.at(EnumFieldName("missing")).isUndefined(), "const missing named at returns undefined");
    check(view[EnumFieldName("missing")].isUndefined(), "const missing named operator[] returns undefined");
    check(map.count() == 1, "const missing named access does not insert");
    check(map.at(EnumFieldName("insert-at")).isNull(), "mutable missing named at inserts null");
    check(map.contains("insert_at") && map.count() == 2, "named at inserts requested normalized key once");
    check(map[EnumFieldName("insert-bracket")].isNull(), "mutable missing named operator[] inserts null");
    check(map.contains("insert_bracket") && map.count() == 3, "named operator[] inserts requested key once");
    const EnumFieldName empty("");
    map.at(empty) = Node::string("empty-key");
    check(view[empty].getString() == "empty-key", "empty single key remains a valid map key");
}

static void test_list_and_array_overloads() {
    Node map = Node::map();
    const QList<EnumFieldName> alternatives{"first", "second"};
    map.at(QList<EnumFieldName>{"second"}) = Node::number(2);
    const Node &view = map;
    check(map.at(alternatives).getNumber() == 2, "mutable list at searches fallback keys");
    check(view.at(alternatives).getNumber() == 2, "const list at searches fallback keys");
    check(map[alternatives].getNumber() == 2, "mutable list operator[] searches fallback keys");
    check(view[alternatives].getNumber() == 2, "const list operator[] searches fallback keys");
    map.at(QList<EnumFieldName>{"first"}) = Node::number(1);
    check(view.at(alternatives).getNumber() == 1, "list key order is unchanged");
    const QList<EnumFieldName> absent{"new-first", "new-second"};
    check(map.at(absent).isNull(), "missing mutable list at inserts null");
    check(map.contains("new-first") && !map.contains("new-second"), "missing list at inserts only first alternative");
    const auto before = map.count();
    check(view.at(QList<EnumFieldName>{"absent"}).isUndefined(), "missing const list at returns undefined");
    check(view.at(QList<EnumFieldName>{}).isUndefined(), "empty const list returns undefined");
    check(map.count() == before, "const list lookups never insert");

    Node array = Node::array();
    array.at(size_t(2)) = Node::number(12);
    const Node &arrayView = array;
    check(array.count() == 3, "mutable numeric at still extends array");
    check(arrayView.at(size_t(0)).isNull() && arrayView.at(size_t(1)).isNull(), "array extension inserts nulls");
    check(arrayView.at(size_t(2)).getNumber() == 12, "const numeric at reads array");
    array[size_t(1)] = Node::number(11);
    check(arrayView[size_t(1)].getNumber() == 11, "numeric operator[] remains distinct from named access");
    check(arrayView.at(size_t(8)).isUndefined(), "const out-of-range array access returns undefined");
    check(array.count() == 3, "const out-of-range array access does not extend");
    check(array.at(EnumFieldName("0")).isUndefined(), "named array at returns undefined");
    check(arrayView[EnumFieldName("0")].isUndefined(), "const named array operator[] returns undefined");
    check(map.at(size_t(0)).isUndefined(), "numeric map at returns undefined");
    check(view[size_t(0)].isUndefined(), "const numeric map operator[] returns undefined");
}

static void test_scalar_indexing() {
    const QList<Node> scalars{Node(), Node::null(), Node(true), Node(false), Node(4.5), Node(QString("text")), Node(EnumFieldName("enum"))};
    for (Node scalar : scalars) {
        const Node &view = scalar;
        const Tag type = scalar.type();
        const EnumFieldName key("anything");
        const QList<EnumFieldName> keys{key};
        check(scalar.at(key).isUndefined(), "mutable scalar named at returns undefined");
        check(view.at(key).isUndefined(), "const scalar named at returns undefined");
        check(scalar[key].isUndefined(), "mutable scalar named operator[] returns undefined");
        check(view[key].isUndefined(), "const scalar named operator[] returns undefined");
        check(scalar.at(keys).isUndefined() && view.at(keys).isUndefined(), "scalar list at returns undefined");
        check(scalar[keys].isUndefined() && view[keys].isUndefined(), "scalar list operator[] returns undefined");
        check(scalar.at(size_t(0)).isUndefined() && view.at(size_t(0)).isUndefined(), "scalar numeric at returns undefined");
        check(scalar[size_t(0)].isUndefined() && view[size_t(0)].isUndefined(), "scalar numeric operator[] returns undefined");
        check(scalar.type() == type && scalar.count() == 0, "failed scalar indexing preserves source type");
    }
}

static void test_production_json_construction() {
    const QByteArray bytes = R"({"Tls-Enabled":true,"disabled":false,"n":1.5,"text":"value","nil":null,"nested":{"Leaf":42},"array":[{"inner":"ok"},false,3]})";
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    check(error.error == QJsonParseError::NoError, "synthetic JSON fixture parses in actual Qt6");
    const Node direct(document.object());
    const Node parsed = Node::parseJsonValue(QJsonValue(document.object()));
    const Node jsonValue{QJsonValue(document.object())};
    const Node variant = Node::fromVariantMap(document.object().toVariantMap());
    for (const Node *map : {&direct, &parsed, &jsonValue, &variant}) {
        check(map->isMap() && map->count() == 7, "production object construction preserves all keys");
        check((*map)[EnumFieldName("tls_enabled")].getBoolean(), "object true survives construction and const lookup");
        check(!map->at(EnumFieldName("disabled")).getBoolean(true), "object false survives construction and const lookup");
        check(map->at(EnumFieldName("n")).getNumber() == 1.5, "object number survives construction");
        check(map->at(EnumFieldName("text")).getString() == "value", "object string survives construction");
        check(map->at(EnumFieldName("nil")).isNull(), "object null survives construction");
        check(map->at(EnumFieldName("nested"))[EnumFieldName("leaf")].getNumber() == 42, "nested production object construction and key access");
        const Node &array = map->at(EnumFieldName("array"));
        check(array.isArray() && array.count() == 3, "nested production array construction");
        check(array.at(size_t(0))[EnumFieldName("inner")].getString() == "ok", "object inside array uses production constructors and overloads");
        check(!array.at(size_t(1)).getBoolean(true) && array.at(size_t(2)).getNumber() == 3, "array scalar entries retain values");
    }
    const Node empty{QJsonObject()};
    check(empty.isMap() && empty.count() == 0, "empty production object constructor remains valid");
    Node copied(direct);
    copied.at(EnumFieldName("n")) = Node::number(8);
    check(direct.at(EnumFieldName("n")).getNumber() == 1.5, "mutable map lookup retains Qt copy-on-write isolation");
    check(copied.at(EnumFieldName("n")).getNumber() == 8, "copied map write updates its own storage");
}

int main() {
    std::cout << "Qt " << qVersion() << ": production Node constructor/index regression\n";
    test_key_overloads();
    test_list_and_array_overloads();
    test_scalar_indexing();
    test_production_json_construction();
    std::cout << "RESULT: " << (checks - failures) << '/' << checks << " checks passed\n";
    return failures ? 1 : 0;
}
