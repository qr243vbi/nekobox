#include "support.hpp"
#include "production_refresh.inc"

static void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

extern "C" const char *run_case(const char *caseName, EventCallback callback) {
    static std::string error;
    error.clear();
    eventCallback = callback;
    warnings = logs = 0;
    warningText.clear();
    Configs::ProfileManager manager;
    Configs::profileManager = &manager;
    Table table;
    Ui ui{&table};
    Model model{&table};
    MainWindow window{&ui, &model};
    const std::string name(caseName);
    try {
        if (name == "missing_group_full" || name == "missing_group_single" ||
            name == "missing_group_direct_helper") {
            if (name == "missing_group_direct_helper") {
                window.refresh_proxy_list_impl_refresh_data(-1);
            } else {
                window.refresh_proxy_list_impl(name == "missing_group_full" ? -1 : 7, {});
            }
            check(table.updatesEnabled, "missing group leaves painting disabled");
            check(model.refreshes == 0 && warnings == 0, "missing group changed refresh/warning behavior");
            check(logs == (name == "missing_group_full" ? 1 : 0), "missing group changed logging behavior");
        } else {
            manager.group = std::make_shared<Configs::Group>();
            auto &ids = manager.group->profiles;
            int id = -1;
            GroupSortAction action;
            bool reject = false;
            int expectedRefreshes = 1;
            if (name.rfind("oversized_", 0) == 0 || name == "boundary_12000") {
                const int count = name == "boundary_12000" ? 12000 : 12001;
                for (int i = count; i > 0; --i) ids.push_back(i);
                action.method = GroupSortMethod::ByName;
                if (name == "oversized_address") action.method = GroupSortMethod::ByAddress;
                if (name == "oversized_type") action.method = GroupSortMethod::ByType;
                if (name == "oversized_latency") action.method = GroupSortMethod::ByLatency;
                if (name == "oversized_traffic") action.method = GroupSortMethod::ByTotalData;
                if (name == "oversized_descending") action.descending = true;
                if (name == "oversized_raw") action.method = GroupSortMethod::Raw;
                if (name == "oversized_id") action.method = GroupSortMethod::ById;
                reject = count > 12000 && action.method != GroupSortMethod::Raw &&
                         action.method != GroupSortMethod::ById;
                expectedRefreshes = reject ? 0 : 1;
            } else if (name == "missing_id_small" || name == "missing_id_3000") {
                for (int i = name == "missing_id_small" ? 2 : 3000; i > 0; --i) ids.push_back(i);
                id = 9000;
                expectedRefreshes = name == "missing_id_small" ? 0 : 1;
            } else if (name != "empty_group") {
                ids = {2, 1};
                for (int value : ids) {
                    auto profile = std::make_shared<Configs::ProxyEntity>();
                    profile->name = value == 1 ? "Alpha" : "Beta";
                    profile->latencyInt = value * 10;
                    manager.profiles[value] = profile;
                }
                if (name == "normal_single") id = 1;
                if (name == "sort_ascending" || name == "sort_descending") {
                    action.method = GroupSortMethod::ByName;
                    action.descending = name == "sort_descending";
                }
            }
            const auto original = ids;
            window.refresh_proxy_list_impl(id, action);
            check(table.updatesEnabled, "refresh/sort exit leaves painting disabled");
            check(!table.signalsBlocked, "refresh leaves table signals blocked");
            check(model.refreshes == expectedRefreshes, "refresh count changed");
            check(logs == 0, "unexpected missing-group log");
            check(warnings == (reject ? 1 : 0), "sort warning threshold/coverage changed");
            if (reject) {
                check(warningText == "Group is too big to sort", "sort warning text changed");
                check(ids == original, "rejected sort reordered profile IDs");
            } else if (name == "sort_ascending") {
                check(ids == Configs::ProfileIds({1, 2}), "ascending sort changed");
            } else if (name == "sort_descending") {
                check(ids == Configs::ProfileIds({2, 1}), "descending sort changed");
            } else if (name != "boundary_12000") {
                check(ids == original, "unsorted refresh reordered profile IDs");
            }
            if (name == "normal_full") {
                check(manager.profiles[1]->latencyOrder == 1 && manager.profiles[2]->latencyOrder == 2,
                      "full refresh no longer calculates latency order");
            }
        }
    } catch (const std::exception &failure) {
        error = failure.what();
    }
    eventCallback = nullptr;
    Configs::profileManager = nullptr;
    return error.c_str();
}
