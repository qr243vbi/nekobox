#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class ChainBean : public AbstractBean {
    public:
        QList<int> list; // in to out

        ChainBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
        }
        
        INIT_BEAN_MAP
            ADD_MAP("list", list, integerList);
        STOP_MAP

  //      QString DisplayType() override { return QObject::tr("Chain Proxy"); };

  //      QString DisplayAddress() override { return ""; };

        virtual QString type()const override {
            return "chain";
        };

    };
} // namespace Configs
