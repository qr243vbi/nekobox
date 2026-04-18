#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "ProxyEntity.hpp"

namespace Configs {
struct ProfileFilterKey
{
    std::shared_ptr<Configs::ProxyEntity> key;

    ProfileFilterKey(const std::shared_ptr<Configs::ProxyEntity>& key,
                     bool unpack_bean) noexcept
        : key(key),
          unpack_bean(unpack_bean)
    {}

    // --- Bean Bytes ---
    QByteArray beanBytes() const noexcept;

    // --- Equality ---
    bool operator==(const ProfileFilterKey &other) const noexcept;
    bool operator!=(const ProfileFilterKey &other) const noexcept;

    // --- Ordering (for QMap / std::map) ---
    bool operator<(const ProfileFilterKey &other) const noexcept;
    bool operator>(const ProfileFilterKey &other) const noexcept;
    bool operator<=(const ProfileFilterKey &other) const noexcept;
    bool operator>=(const ProfileFilterKey &other) const noexcept;

private:
    mutable bool unpack_bean;
    mutable QByteArray cache{};
};

// --- Hash ---
inline uint qHash(const ProfileFilterKey &key, uint seed = 0) noexcept;

// --- Helper factory ---
ProfileFilterKey ProfileFilter_ent_key(
    const std::shared_ptr<Configs::ProxyEntity> &ent,
    bool by_address
);

    class ProfileFilter {
    public:
        static void Uniq(
            const QList<std::shared_ptr<ProxyEntity>> &in,
            QList<std::shared_ptr<ProxyEntity>> &out,
            bool by_address = false, // def by bean
            bool keep_last = false   // def keep first
        );

        static void Common(
            const QList<std::shared_ptr<ProxyEntity>> &src,
            const QList<std::shared_ptr<ProxyEntity>> &dst,
            QList<std::shared_ptr<ProxyEntity>> &outSrc,
            QList<std::shared_ptr<ProxyEntity>> &outDst,
            bool by_address = false // def by bean
        );

        static void OnlyInSrc(
            const QList<std::shared_ptr<ProxyEntity>> &src,
            const QList<std::shared_ptr<ProxyEntity>> &dst,
            QList<std::shared_ptr<ProxyEntity>> &out,
            bool by_address = false // def by bean
        );

        static void OnlyInSrc_ByPointer(
            const QList<std::shared_ptr<ProxyEntity>> &src,
            const QList<std::shared_ptr<ProxyEntity>> &dst,
            QList<std::shared_ptr<ProxyEntity>> &out);
    };
} // namespace Configs
