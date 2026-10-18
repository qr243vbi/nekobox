



#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class ChainBean : public AbstractBean {
    public:
        QList<int> list; // in to out
        // set by the importer: which input entry the chain stands for
        QString import_key;

        ChainBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
        }
        
        INIT_BEAN_MAP
            ADD_MAP("list", list, integerList);
            ADD_MAP("import", import_key, string);
        STOP_MAP

  //      QString DisplayType() override { return QObject::tr("Chain Proxy"); };

  //      QString DisplayAddress() override { return ""; };

        virtual QString type()const override {
            return "chain";
        };

    };
} // namespace Configs
