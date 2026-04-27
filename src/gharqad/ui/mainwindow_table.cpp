#include "nekobox/dataStore/ProxyEntity.hpp"
#include <nekobox/ui/mainwindow_table.h>
#include <nekobox/ui/mainwindow.h>
#include <nekobox/global/GuiUtils.hpp>
#include <nekobox/dataStore/Database.hpp>

MyTableModel::MyTableModel(QObject *parent): QAbstractTableModel(parent){
};


int MyTableModel::columnCount(const QModelIndex &parent) const {
    return 5;
};

static std::shared_ptr<Configs::ProxyEntity> getProxyRow(int row){
    return Configs::profileManager->GetProfile(row);
}

int MyTableModel::data_id(const QModelIndex &index) const
{
    #ifdef DEBUG_MODE
    qDebug() << "Valid Index? " << index.isValid();
    #endif

    if (!index.isValid())
        return -1;

    int column = index.column();
    int row = index.row();

    #ifdef DEBUG_MODE
    qDebug() << "Row Id" << row;
    #endif

    if (!(column < 5 && row < this->m_data()->profiles.count())){
        return -1;
    }
    return this->m_data()->profiles[row];
}

QVariant MyTableModel::data(const QModelIndex &index, int role) const
{
    int row = data_id(index);
    #ifdef DEBUG_MODE
    qDebug() << "Row Id" << row;
    #endif
    if (row < 0){
        return QVariant();
    }

    if (role != Qt::DisplayRole){
        if (role == Qt::ForegroundRole){
            switch (index.column()){
                case 4: {
                    const std::shared_ptr<Configs::ProxyEntity> person = getProxyRow(row);
                    return QBrush(DisplayLatencyColor(person.get()));
                }
                default:
                break;
            }
        } 
        return QVariant();
    }

    const std::shared_ptr<Configs::ProxyEntity> person = getProxyRow(row);

    int column = index.column()+ 1;
    switch (column) {
        case 1:
            return person->DisplayType();
        case 2:
            return person->DisplayAddress();
        case 3:
            return person->DisplayName();
        case 4:
            if (person->full_test_report.isEmpty()) {
                return person->DisplayTestResult();
            } else {
                return person->full_test_report;
            }
        case 5:
            return person->traffic_data->DisplayTraffic();
        default:
        break;
    }
    return QVariant();
}

int MyTableModel::rowCount(const QModelIndex &parent) const {
    return this->m_data()->profiles.count();
};

std::shared_ptr<Configs::Group> MyTableModel::m_data() const {
    return Configs::profileManager->CurrentGroup();
}

QVariant MyTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section){
            case 0:
            return tr("Type");
            case 1:
            return tr("Address");
            case 2:
            return tr("Name");
            case 3:
            return tr("Test Result");
            case 4:
            return tr("Traffic");
        }
    }
    return QVariant();
}