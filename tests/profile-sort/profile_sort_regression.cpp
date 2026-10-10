#include "native_fixture.h"
#include <QSignalSpy>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
struct Fixture {
    Configs::ProfileManager manager;
    MyTableModel source;
    ColumnFilterProxy proxy;
    QTableView view;
    std::unique_ptr<SelectionKeeper> keeper;
    SyntheticUi ui;
    MainWindow window;

    explicit Fixture(QList<int> ids = {101, 102, 103}, bool filtered = false, bool keepSelection = true) {
        Configs::profileManager = &manager;
        manager.group->profiles = ids;
        for (int id : {101, 102, 103, 104}) {
            auto profile = std::make_shared<Configs::ProxyEntity>();
            profile->type = id == 102 ? "udp" : "tcp";
            profile->address = id == 101 || id == 104 ? "east" : "west";
            profile->name = id == 101 ? "keep charlie" : id == 102 ? "skip alpha" : "keep bravo";
            profile->latencyInt = id == 101 ? 30 : id == 102 ? 10 : 20;
            profile->traffic_data->downlink = id == 101 ? 100 : id == 102 ? 300 : 200;
            manager.profiles[id] = profile;
        }
        proxy.setSourceModel(&source);
        proxy.setDynamicSortFilter(false);
        if (filtered) proxy.setGlobalFilter("keep");
        view.setModel(filtered ? static_cast<QAbstractItemModel *>(&proxy) : &source);
        source.m_view = &view;
        source.refresh(); // Initialize old_count before testing same-count sorts.
        if (keepSelection) keeper = std::make_unique<SelectionKeeper>(&view);
        ui.proxyListTable = &view;
        window.ui = &ui;
        window.tableModel = &source;
    }
    void sort(bool descending = false, GroupSortMethod::GroupSortMethod method = GroupSortMethod::ByName) {
        GroupSortAction action;
        action.method = method;
        action.descending = descending;
        window.refresh_proxy_list_impl(-1, action);
    }
    void select(const QList<int> &ids, int current) {
        auto *model = view.model();
        auto *sm = view.selectionModel();
        for (int row = 0; row < model->rowCount(); ++row) {
            const auto index = model->index(row, 0);
            const int id = index.data(SELECTION_KEEPER_ROLE).toInt();
            if (ids.contains(id)) sm->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            if (id == current) sm->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        }
    }
    void checkSelection(QList<int> expected, int current) {
        QList<int> actual;
        for (const auto &index : view.selectionModel()->selectedRows()) actual.append(index.data(SELECTION_KEEPER_ROLE).toInt());
        std::sort(actual.begin(), actual.end());
        std::sort(expected.begin(), expected.end());
        require(actual == expected, "selected profile identities changed or gained stale rows");
        const auto index = view.selectionModel()->currentIndex();
        require(current < 0 ? !index.isValid() : index.data(SELECTION_KEEPER_ROLE).toInt() == current,
                "current profile identity changed");
    }
    QList<int> visible() {
        QList<int> ids;
        auto *model = view.model();
        for (int row = 0; row < model->rowCount(); ++row) {
            const auto index = model->index(row, 0);
            ids.append(index.data(SELECTION_KEEPER_ROLE).toInt());
            if (model == &proxy) {
                const auto src = proxy.mapToSource(index);
                require(src.isValid() && proxy.mapFromSource(src) == index, "proxy mapping is not reversible");
            }
        }
        return ids;
    }
};
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    const std::vector<std::pair<const char *, std::function<void()>>> tests = {
        {"single selection/current/all columns survive both directions", [] {
            Fixture f;
            f.select({101}, 101);
            QList<QPersistentModelIndex> indexes;
            for (int col = 0; col < 5; ++col) indexes.append(f.source.index(0, col));
            for (bool descending : {false, true, false}) {
                f.sort(descending);
                f.checkSelection({101}, 101);
                for (int col = 0; col < 5; ++col) {
                    require(indexes[col].data(SELECTION_KEEPER_ROLE).toInt() == 101, "source persistent identity changed");
                    require(indexes[col].column() == col, "persistent column changed");
                }
            }
        }},
        {"multi-selection and equal-key profiles survive", [] {
            Fixture f({101, 102, 103, 104});
            f.select({101, 102, 104}, 104);
            f.sort(); f.checkSelection({101, 102, 104}, 104);
            f.sort(true); f.checkSelection({101, 102, 104}, 104);
        }},
        {"established global-filter cache and proxy indexes survive", [] {
            Fixture f({101, 102, 103}, true);
            require(f.visible() == QList<int>({101, 103}), "initial filter fixture invalid");
            f.select({101}, 101);
            const QPersistentModelIndex index(f.proxy.index(0, 2));
            for (bool descending : {false, true, false}) {
                f.sort(descending);
                require(f.visible() == (descending ? QList<int>{101, 103} : QList<int>{103, 101}), "filtered membership/order is stale");
                f.checkSelection({101}, 101);
                require(index.data(SELECTION_KEEPER_ROLE).toInt() == 101 && index.column() == 2, "proxy persistent identity changed");
                require(!f.proxy.mapFromSource(f.source.index(f.manager.group->profiles.indexOf(102), 0)).isValid(), "nonmatching profile appears in proxy");
                require(!f.proxy.dynamicSortFilter(), "fixture changed production dynamicSortFilter setting");
            }
        }},
        {"combined column/global-filter membership stays correct", [] {
            Fixture f({101, 102, 103}, true);
            f.proxy.setEnabled(true);
            f.proxy.setColumnFilter(1, "east");
            require(f.visible() == QList<int>({101}), "initial column filter fixture invalid");
            f.select({101}, 101);
            f.sort(); require(f.visible() == QList<int>({101}), "column filter became stale"); f.checkSelection({101}, 101);
            f.sort(true); require(f.visible() == QList<int>({101}), "reverse column filter became stale"); f.checkSelection({101}, 101);
        }},
        {"empty selection stays empty", [] {
            Fixture f;
            f.sort(); f.checkSelection({}, -1);
            f.sort(true); f.checkSelection({}, -1);
        }},
        {"empty/single/already-sorted groups emit no layout/reset", [] {
            for (const auto &ids : {QList<int>{}, QList<int>{101}, QList<int>{103, 101, 102}}) {
                Fixture f(ids);
                QSignalSpy layout(&f.source, &QAbstractItemModel::layoutChanged);
                QSignalSpy reset(&f.source, &QAbstractItemModel::modelReset);
                f.sort();
                require(layout.isEmpty() && reset.isEmpty(), "no-op sort emits structural changes");
                require(f.manager.group->profiles == ids, "no-op sort changed profiles");
            }
        }},
        {"duplicate IDs retain distinct persistent rows and columns", [] {
            Fixture f({101, 102, 101, 103, 104}, false, false);
            QList<QPersistentModelIndex> indexes;
            QList<int> identities = f.manager.group->profiles;
            for (int row = 0; row < identities.size(); ++row) indexes.append(f.source.index(row, row % 5));
            for (bool descending : {false, true}) {
                f.sort(descending);
                QSet<int> rows;
                for (int i = 0; i < indexes.size(); ++i) {
                    require(indexes[i].data(SELECTION_KEEPER_ROLE).toInt() == identities[i], "duplicate/equal-key persistent identity changed");
                    require(indexes[i].column() == i % 5, "duplicate persistent column changed");
                    rows.insert(indexes[i].row());
                }
                require(rows.size() == indexes.size(), "duplicate IDs collapsed persistent rows");
            }
        }},
        {"about-to slot-created persistent indexes are captured", [] {
            Fixture f({101, 102, 103}, false, false);
            QPersistentModelIndex created;
            QStringList events;
            QObject::connect(&f.source, &QAbstractItemModel::layoutAboutToBeChanged, &f.source, [&] {
                require(f.manager.group->profiles == QList<int>({101, 102, 103}), "about-to signal emitted after mutation");
                created = f.source.index(0, 4);
                events.append("before");
            });
            QObject::connect(&f.source, &QAbstractItemModel::layoutChanged, &f.source, [&] { events.append("after"); });
            QSignalSpy reset(&f.source, &QAbstractItemModel::modelReset);
            f.sort();
            require(events == QStringList({"before", "after"}), "sort did not emit one ordered layout pair");
            require(created.isValid() && created.data(SELECTION_KEEPER_ROLE).toInt() == 101 && created.column() == 4,
                    "persistent index captured before about-to slots ran");
            require(reset.isEmpty(), "sort must not reset the model");
        }},
        {"all five header comparators keep selected identity", [] {
            Fixture f;
            f.select({101}, 101);
            for (auto method : {GroupSortMethod::ByType, GroupSortMethod::ByAddress, GroupSortMethod::ByName,
                                GroupSortMethod::ByLatency, GroupSortMethod::ByTotalData}) {
                f.sort(false, method); f.checkSelection({101}, 101);
                f.sort(true, method); f.checkSelection({101}, 101);
            }
        }},
        {"raw/id actions and oversized-sort guard keep existing behavior", [] {
            Fixture f;
            const auto original = f.manager.group->profiles;
            f.sort(false, GroupSortMethod::Raw); require(f.manager.group->profiles == original, "Raw action changed");
            f.sort(false, GroupSortMethod::ById); require(f.manager.group->profiles == original, "ById action changed");
            f.manager.group->profiles.clear();
            for (int i = 0; i < 12001; ++i) f.manager.group->profiles.append(i);
            const auto oversized = f.manager.group->profiles;
            const int before = warnings;
            f.sort();
            require(warnings == before + 1, "oversized group warning changed");
            require(f.manager.group->profiles == oversized, "oversized group sorted");
        }},
    };
    int failures = 0;
    for (const auto &[name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception &error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    std::cout << tests.size() << " cases, " << failures << " failures; extracted production on Qt " << qVersion() << '\n';
    return failures ? 1 : 0;
}
