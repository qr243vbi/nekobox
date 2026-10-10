#include "column_filter_proxy.h"
#include <QAbstractTableModel>
#include <QCoreApplication>
#include <QStringList>
#include <QVector>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
constexpr int IdRole = Qt::UserRole + 3;
struct Row { int id; QStringList values; };
const QVector<Row> Rows = {
    {101, {"Poland Alpha", "east", "tcp"}},
    {102, {"Germany Beta", "west", "udp"}},
    {103, {"Poland Beta", "west", "tcp"}},
};

void require(bool condition, const QString &message)
{
    if (!condition) throw std::runtime_error(message.toStdString());
}

QString idsString(const QVector<int> &ids)
{
    QStringList result;
    for (int id : ids) result.append(QString::number(id));
    return "[" + result.join(", ") + "]";
}

class FixedSource : public QAbstractTableModel
{
public:
    int rowCount(const QModelIndex &parent = {}) const override
    { return parent.isValid() ? 0 : Rows.size(); }
    int columnCount(const QModelIndex &parent = {}) const override
    { return parent.isValid() ? 0 : 3; }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid()) return {};
        if (role == Qt::DisplayRole) return Rows.at(index.row()).values.at(index.column());
        if (role == IdRole) return Rows.at(index.row()).id;
        return {};
    }
};

struct Fixture {
    FixedSource source;
    ColumnFilterProxy proxy;
    int sourceChanges = 0;
    Fixture()
    {
        proxy.setSourceModel(&source);
        proxy.setDynamicSortFilter(false);
        QObject::connect(&source, &QAbstractItemModel::modelReset, &source, [this] { ++sourceChanges; });
        QObject::connect(&source, &QAbstractItemModel::layoutChanged, &source, [this] { ++sourceChanges; });
        QObject::connect(&source, &QAbstractItemModel::dataChanged, &source, [this] { ++sourceChanges; });
        QObject::connect(&source, &QAbstractItemModel::rowsInserted, &source, [this] { ++sourceChanges; });
        QObject::connect(&source, &QAbstractItemModel::rowsRemoved, &source, [this] { ++sourceChanges; });
    }
    void check(const QVector<int> &expected)
    {
        require(source.rowCount() == 3, "source row count must remain three");
        require(sourceChanges == 0, "source changes must not rescue stale mapping");
        require(!proxy.dynamicSortFilter(), "dynamic filtering must stay disabled");
        QVector<int> actual;
        for (int row = 0; row < proxy.rowCount(); ++row) {
            const auto proxyIndex = proxy.index(row, 0);
            const auto sourceIndex = proxy.mapToSource(proxyIndex);
            require(sourceIndex.isValid(), "invalid source mapping");
            require(proxy.mapFromSource(sourceIndex) == proxyIndex, "mapping must round-trip");
            actual.append(source.data(sourceIndex, IdRole).toInt());
        }
        require(actual == expected, "expected " + idsString(expected) + ", got " + idsString(actual));
        for (int row = 0; row < source.rowCount(); ++row)
            require(proxy.mapFromSource(source.index(row, 0)).isValid() == expected.contains(Rows.at(row).id),
                    "visible and excluded source mappings disagree");
    }
};
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    std::cout << "Extracted production ColumnFilterProxy; real Qt " << qVersion() << '\n';
    const QVector<QPair<const char *, std::function<void()>>> cases = {
        {"global changes and clears reuse established mapping", [] {
            Fixture f;
            f.proxy.setGlobalFilter("pOLaNd"); f.check({101, 103});
            f.proxy.setGlobalFilter("beta"); f.check({102, 103});
            f.proxy.setGlobalFilter(""); f.check({101, 102, 103});
            f.proxy.setGlobalFilter("EaSt"); f.check({101});
            f.proxy.setGlobalFilter("absent"); f.check({});
            f.proxy.setGlobalFilter("Poland"); f.check({101, 103});
            f.proxy.setGlobalFilter(""); f.check({101, 102, 103});
        }},
        {"column changes and clears reuse established mapping", [] {
            Fixture f;
            f.proxy.setEnabled(true);
            f.proxy.setColumnFilter(0, "Poland"); f.check({101, 103});
            f.proxy.setColumnFilter(0, "bETA"); f.check({102, 103});
            f.proxy.setColumnFilter(0, ""); f.check({101, 102, 103});
            f.proxy.setColumnFilter(0, "absent"); f.check({});
            f.proxy.setColumnFilter(0, "Alpha"); f.check({101});
            f.proxy.setColumnFilter(0, ""); f.check({101, 102, 103});
        }},
        {"enabling columns retains active global filter", [] {
            Fixture f;
            f.proxy.setGlobalFilter("beta");
            f.proxy.setColumnFilter(0, "Poland"); f.check({102, 103});
            f.proxy.setEnabled(true); f.check({103});
        }},
        {"disabling clears columns but retains global filter", [] {
            Fixture f;
            f.proxy.setEnabled(true);
            f.proxy.setGlobalFilter("beta");
            f.proxy.setColumnFilter(0, "Poland"); f.check({103});
            f.proxy.setEnabled(false); f.check({102, 103});
            f.proxy.setEnabled(true); f.check({102, 103});
            f.proxy.setColumnFilter(0, "Germany"); f.check({102});
            f.proxy.setEnabled(false); f.check({102, 103});
        }},
        {"global any-column and per-column filters intersect", [] {
            Fixture f;
            f.proxy.setEnabled(true);
            f.proxy.setGlobalFilter("beta");
            f.proxy.setColumnFilter(2, "tcp"); f.check({103});
            f.proxy.setColumnFilter(0, "Germany"); f.check({});
            f.proxy.setColumnFilter(2, ""); f.check({102});
            f.proxy.setColumnFilter(0, ""); f.check({102, 103});
            f.proxy.setGlobalFilter("EAST"); f.check({101});
            f.proxy.setGlobalFilter(""); f.check({101, 102, 103});
        }},
        {"columns default disabled and repeated disable clears them", [] {
            Fixture f;
            f.check({101, 102, 103});
            f.proxy.setColumnFilter(0, "Poland"); f.check({101, 102, 103});
            f.proxy.setEnabled(false); f.check({101, 102, 103});
            f.proxy.setEnabled(true); f.check({101, 102, 103});
        }},
    };
    int failures = 0;
    for (const auto &test : cases) {
        try {
            test.second();
            std::cout << "PASS: " << test.first << '\n';
        } catch (const std::exception &error) {
            ++failures;
            std::cerr << "FAIL: " << test.first << ": " << error.what() << '\n';
        }
    }
    std::cout << cases.size() << " cases, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
