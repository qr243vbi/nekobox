#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class ChainBean : public AbstractBean {
    public:
        QList<int> list; // in to out

        ChainBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
        }
        
        INIT_MAP
            ADD_MAP("list", list, integerList);
        STOP_MAP

  //      QString DisplayType() override { return QObject::tr("Chain Proxy"); };

  //      QString DisplayAddress() override { return ""; };
  #ifdef DEBUG_MODE
        virtual QString type() override {
            return "chain";
        };
  #endif
    };
} // namespace Configs
