#pragma once
// Dependency seam for the portable, source-linked ProfileFilter regression test.
// No Qt, database, profile files, credentials, network, or core process is used.
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

template <typename T> class QList : public std::vector<T> {
public:
    using std::vector<T>::vector;
    QList &operator+=(const T &value) { this->push_back(value); return *this; }
    void removeAll(const T &value) {
        this->erase(std::remove(this->begin(), this->end(), value), this->end());
    }
    bool contains(const T &value) const {
        return std::find(this->begin(), this->end(), value) != this->end();
    }
};

namespace Configs {
struct SyntheticBean {
    std::string credential = "synthetic-A";
    std::string transport = "tcp";
    std::string config = "synthetic-config-A";
    std::string custom_config;
    std::string custom_outbound;

    int compare(const SyntheticBean *other, const QList<std::string> &skip = {}) const {
        const auto fields = [&](const SyntheticBean &bean) {
            return std::make_tuple(bean.credential, bean.transport, bean.config,
                skip.contains("c_cfg") ? std::string{} : bean.custom_config,
                skip.contains("c_out") ? std::string{} : bean.custom_outbound);
        };
        const auto a = fields(*this), b = fields(*other);
        return a < b ? -1 : (b < a ? 1 : 0);
    }
};

struct ProxyEntity {
    std::string type = "vless";
    std::string serverAddress = "a.example.invalid";
    int serverPort = 443;
    int id = 0;
    std::string name = "Synthetic profile";
    std::shared_ptr<SyntheticBean> contents = std::make_shared<SyntheticBean>();

    std::shared_ptr<const SyntheticBean> bean() const { return contents; }
    int Id() const { return id; }
    // Models the source boundary: ProxyEntity::compare compares entity metadata,
    // not the separate AbstractBean fields. The bean is intentionally absent.
    int compare(const ProxyEntity *other, const QList<std::string> & = {}) const {
        const auto a = std::tie(type, serverAddress, serverPort, id, name);
        const auto b = std::tie(other->type, other->serverAddress, other->serverPort,
                                other->id, other->name);
        return a < b ? -1 : (b < a ? 1 : 0);
    }
};
} // namespace Configs
