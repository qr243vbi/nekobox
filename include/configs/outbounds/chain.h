#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class chain : public outbound
    {
        public:
        QList<int> list; // from in to out

        chain() : outbound()
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("list", &list, integerList));
        }

        QString DisplayType() override { return QObject::tr("Chain Proxy"); };

        QString DisplayAddress() override { return ""; };

        BuildResult Build() override
        {
            return {{}, "Cannot call Build on chain config"};
        }
    };
}
