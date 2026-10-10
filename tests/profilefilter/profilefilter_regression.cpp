#include <nekobox/dataStore/ProfileFilter.hpp>
#include <iostream>
#include <stdexcept>
#include <string>

using Configs::ProfileFilter;
using Configs::ProfileFilterKey;
using Configs::ProfileFilter_ent_key;
using Configs::ProxyEntity;
using Profile = std::shared_ptr<ProxyEntity>;

static void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
static Profile make_profile(int id = 0) {
    auto result = std::make_shared<ProxyEntity>();
    result->id = id;
    return result;
}
static void expect_profiles(const QList<Profile> &actual,
                            const QList<Profile> &expected, const char *message) {
    require(actual == expected, message);
}
static QList<Profile> unique(const QList<Profile> &input, bool by_address = false,
                            bool keep_last = false) {
    QList<Profile> out;
    ProfileFilter::Uniq(input, out, by_address, keep_last);
    return out;
}

static void ordering_cross_fields() {
    auto a = make_profile(), b = make_profile();
    a->serverAddress = "a.example.invalid";
    a->serverPort = 8443;
    b->serverAddress = "b.example.invalid";
    b->serverPort = 443;
    const ProfileFilterKey ka(a, true), kb(b, true);
    require(ka < kb, "address should decide before port");
    require(!(kb < ka), "crossed address/port values compare less in both directions");
    a->type = "vless"; b->type = "socks";
    require(kb < ka && !(ka < kb), "type should decide before address");
}

static void ordering_laws() {
    std::vector<ProfileFilterKey> keys;
    keys.emplace_back(nullptr, false);
    keys.emplace_back(nullptr, true);
    for (const auto &type : {"socks", "vless"})
      for (const auto &address : {"a.example.invalid", "b.example.invalid"})
        for (int port : {443, 8443})
          for (const auto &credential : {"synthetic-A", "synthetic-B"})
            for (bool skip : {false, true}) {
                auto p = make_profile();
                p->type = type; p->serverAddress = address; p->serverPort = port;
                p->contents->credential = credential;
                keys.emplace_back(p, skip);
            }
    const auto equivalent = [](const auto &a, const auto &b) {
        return !(a < b) && !(b < a);
    };
    for (const auto &a : keys) {
        require(!(a < a), "irreflexivity violated");
        for (const auto &b : keys) {
            require(!(a < b && b < a), "asymmetry violated");
            require(equivalent(a, b) == (a == b), "ordering/equality disagree");
            require((a != b) == !(a == b), "inequality disagrees");
            require((a > b) == (b < a), "greater-than disagrees");
            require((a <= b) == !(b < a), "less-equal disagrees");
            require((a >= b) == !(a < b), "greater-equal disagrees");
            for (const auto &c : keys) {
                require(!(a < b && b < c) || a < c, "ordering transitivity violated");
                require(!(equivalent(a,b) && equivalent(b,c)) || equivalent(a,c),
                        "equivalence transitivity violated");
            }
        }
    }
}

static void credentials_retained() {
    auto a = make_profile(), b = make_profile();
    b->contents->credential = "synthetic-B";
    expect_profiles(unique({a,b}), {a,b}, "different credentials were removed");
    require(!(ProfileFilter_ent_key(a,false) == ProfileFilter_ent_key(b,false)),
            "full-profile equality ignored different credentials");
}
static void transport_retained() {
    auto a = make_profile(), b = make_profile();
    b->contents->transport = "ws";
    expect_profiles(unique({a,b}), {a,b}, "different transports were removed");
}
static void metadata_ignored() {
    auto a = make_profile(1), b = make_profile(2);
    b->name = "A different display name";
    expect_profiles(unique({a,b}), {a}, "same bean with different metadata not deduplicated");
    require(ProfileFilter_ent_key(a,false) == ProfileFilter_ent_key(b,false),
            "entity metadata incorrectly affects full-profile equality");
}
static void by_address_ignores_bean() {
    auto a = make_profile(1), b = make_profile(2);
    b->contents->credential = "synthetic-B";
    b->contents->transport = "ws";
    expect_profiles(unique({a,b},true), {a}, "address-only mode compared bean/metadata");
}
static void custom_compares_bean() {
    auto a = make_profile(), b = make_profile();
    a->type = b->type = "custom";
    b->contents->config = "synthetic-config-B";
    for (bool by_address : {false,true})
        expect_profiles(unique({a,b},by_address), {a,b}, "custom configs were merged");
    b->contents->config = a->contents->config;
    b->id = 20; b->name = "Renamed custom duplicate";
    expect_profiles(unique({a,b},true), {a}, "identical custom beans not deduplicated");
}
static void endpoint_fields_retained() {
    auto a = make_profile(), type = make_profile(), address = make_profile(), port = make_profile();
    type->type = "socks";
    address->serverAddress = "b.example.invalid";
    port->serverPort = 8443;
    for (bool by_address : {false,true})
        expect_profiles(unique({a,type,address,port},by_address), {a,type,address,port},
                        "distinct endpoint field was ignored");
}
static void keep_first_and_last() {
    auto a = make_profile(1), b = make_profile(2), c = make_profile(3), d = make_profile(4);
    b->contents->credential = "synthetic-B";
    for (bool last : {false,true})
        expect_profiles(unique({a,b,c,d},false,last), last ? QList<Profile>{b,d} : QList<Profile>{a,b},
                        "duplicate groups did not retain the requested representative/order");
    expect_profiles(unique({}), {}, "empty input not empty");
    expect_profiles(unique({a}), {a}, "singleton not retained");
}
static void exclusions_preserved() {
    auto a = make_profile(1), b = make_profile(2);
    b->contents->custom_config = "synthetic override config";
    b->contents->custom_outbound = "synthetic override outbound";
    expect_profiles(unique({a,b}), {a}, "existing c_cfg/c_out exclusions changed");
    require(ProfileFilter_ent_key(a,false) == ProfileFilter_ent_key(b,false),
            "equality did not preserve existing exclusions");
}
static void common_matches_by_bean() {
    auto a = make_profile(1), different = make_profile(2), same = make_profile(3);
    different->contents->credential = "synthetic-B";
    QList<Profile> out_src, out_dst;
    ProfileFilter::Common({a},{different,same},out_src,out_dst,false);
    expect_profiles(out_src,{a},"Common matched different credentials");
    expect_profiles(out_dst,{same},"Common did not find same bean with different metadata");
}
static void permutation_invariance() {
    auto a = make_profile(1), b = make_profile(2), duplicate = make_profile(3), c = make_profile(4);
    a->serverPort = duplicate->serverPort = 8443;
    b->serverAddress = "b.example.invalid";
    c->contents->transport = "ws";
    std::vector<int> order{0,1,2,3};
    QList<Profile> profiles{a,b,duplicate,c};
    do {
        QList<Profile> input;
        for (int i : order) input += profiles[i];
        auto out = unique(input);
        require(out.size() == 3, "insertion order changed the number of duplicate groups");
        int equivalent_count = 0;
        for (const auto &p : out) if (p == a || p == duplicate) ++equivalent_count;
        require(equivalent_count == 1, "duplicate group did not retain exactly one member");
    } while (std::next_permutation(order.begin(), order.end()));
}

int main(int argc, char **argv) {
    const std::map<std::string, void(*)()> tests{
        {"ordering_cross_fields",ordering_cross_fields}, {"ordering_laws",ordering_laws},
        {"credentials_retained",credentials_retained}, {"transport_retained",transport_retained},
        {"metadata_ignored",metadata_ignored}, {"by_address_ignores_bean",by_address_ignores_bean},
        {"custom_compares_bean",custom_compares_bean}, {"endpoint_fields_retained",endpoint_fields_retained},
        {"keep_first_and_last",keep_first_and_last}, {"exclusions_preserved",exclusions_preserved},
        {"common_matches_by_bean",common_matches_by_bean}, {"permutation_invariance",permutation_invariance}
    };
    try {
        require(argc == 2, "pass one test name");
        tests.at(argv[1])();
        std::cout << "PASS " << argv[1] << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << (argc > 1 ? argv[1] : "test") << ": " << error.what() << '\n';
        return 1;
    }
}
