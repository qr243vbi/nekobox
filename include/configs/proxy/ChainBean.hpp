#pragma once

#include "include/global/Configs.hpp"
#include "AbstractBean.hpp"

namespace Configs {
    class ChainBean : public AbstractBean {
    public:
        QList<int> list; // in to out

        ChainBean() : AbstractBean(0) {
        }
        
        INIT_MAP
            ADD_MAP("list", list, integerList);
        STOP_MAP

        QString DisplayType() override { return QObject::tr("Chain Proxy"); };

        QString DisplayAddress() override { return ""; };
    };
} // namespace Configs
