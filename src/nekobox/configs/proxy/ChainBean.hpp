#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class ChainBean : public AbstractBean {
    public:
        QList<int> list; // in to out

<<<<<<< HEAD
        ChainBean() : AbstractBean(0) {
=======
        ChainBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
>>>>>>> other-repo/main
        }
        
        INIT_MAP
            ADD_MAP("list", list, integerList);
        STOP_MAP

<<<<<<< HEAD
        QString DisplayType() override { return QObject::tr("Chain Proxy"); };

        QString DisplayAddress() override { return ""; };
=======
  //      QString DisplayType() override { return QObject::tr("Chain Proxy"); };

  //      QString DisplayAddress() override { return ""; };
  #ifdef DEBUG_MODE
        virtual QString type()const override {
            return "chain";
        };
  #endif
>>>>>>> other-repo/main
    };
} // namespace Configs
