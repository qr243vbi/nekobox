#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProfileFilter.hpp>

namespace Configs {

// --- Bean Bytes (lazy + cached) ---
QByteArray ProfileFilterKey::beanBytes() const noexcept
{
    if (!unpack_bean)
        return cache;

    unpack_bean = false;
    cache = key->bean()->ToBytes({"c_cfg", "c_out"});
    return cache;
}

// --- Equality ---
bool ProfileFilterKey::operator==(const ProfileFilterKey &other) const noexcept
{
    return key->DisplayType() == other.key->DisplayType()
        && key->DisplayAddress() == other.key->DisplayAddress()
        && beanBytes() == other.beanBytes();
}

bool ProfileFilterKey::operator!=(const ProfileFilterKey &other) const noexcept
{
    return !(*this == other);
}

// --- Ordering (STRICT WEAK ORDERING) ---
bool ProfileFilterKey::operator<(const ProfileFilterKey &other) const noexcept
{
    if (key->DisplayType() < other.key->DisplayType())
        return true;

    if (key->DisplayAddress() < other.key->DisplayAddress())
        return true;

    return beanBytes() < other.beanBytes();
}

bool ProfileFilterKey::operator>(const ProfileFilterKey &other) const noexcept
{
    return other < *this;
}

bool ProfileFilterKey::operator<=(const ProfileFilterKey &other) const noexcept
{
    return !(other < *this);
}

bool ProfileFilterKey::operator>=(const ProfileFilterKey &other) const noexcept
{
    return !(*this < other);
}

// --- Hash ---
inline uint qHash(const ProfileFilterKey &key, uint seed) noexcept
{
    seed = qHash(key.key->DisplayType(), seed);
    seed = qHash(key.key->DisplayAddress(), seed);
    seed = qHash(key.beanBytes(), seed);
    return seed;
}

// --- Helper factory ---
ProfileFilterKey ProfileFilter_ent_key(
    const std::shared_ptr<Configs::ProxyEntity> &ent,
    bool by_address)
{
    const bool useAddressOnly = by_address && ent->type != "custom";
    return ProfileFilterKey(ent, !useAddressOnly);
}


void ProfileFilter::Uniq(const QList<std::shared_ptr<ProxyEntity>> &in,
                         QList<std::shared_ptr<ProxyEntity>> &out,
                         bool by_address, bool keep_last) {
  QMap<ProfileFilterKey, std::shared_ptr<ProxyEntity>> hashMap;

  for (const auto &ent : in) {
    ProfileFilterKey key = ProfileFilter_ent_key(ent, by_address);
    if (hashMap.contains(key)) {
      if (keep_last) {
        out.removeAll(hashMap[key]);
        hashMap[key] = ent;
        out += ent;
      }
    } else {
      hashMap[key] = ent;
      out += ent;
    }
  }
}

void ProfileFilter::Common(const QList<std::shared_ptr<ProxyEntity>> &src,
                           const QList<std::shared_ptr<ProxyEntity>> &dst,
                           QList<std::shared_ptr<ProxyEntity>> &outSrc,
                           QList<std::shared_ptr<ProxyEntity>> &outDst,
                           bool by_address) {
  QHash<ProfileFilterKey, std::shared_ptr<ProxyEntity>> map;

  for (const auto &ent : src) {
    map.insert(ProfileFilter_ent_key(ent, by_address), ent);
  }

  for (const auto &ent : dst) {
    const ProfileFilterKey key = ProfileFilter_ent_key(ent, by_address);
    if (map.contains(key)) {
      outDst += ent;
      outSrc += map.value(key);
    }
  }
}

void ProfileFilter::OnlyInSrc(const QList<std::shared_ptr<ProxyEntity>> &src,
                              const QList<std::shared_ptr<ProxyEntity>> &dst,
                              QList<std::shared_ptr<ProxyEntity>> &out,
                              bool by_address) {
  QSet<ProfileFilterKey> keys;
  for (const auto &ent : dst)
    keys.insert(ProfileFilter_ent_key(ent, by_address));

  for (const auto &ent : src) {
    if (!keys.contains(ProfileFilter_ent_key(ent, by_address)))
      out += ent;
  }
}

void ProfileFilter::OnlyInSrc_ByPointer(
    const QList<std::shared_ptr<ProxyEntity>> &src,
    const QList<std::shared_ptr<ProxyEntity>> &dst,
    QList<std::shared_ptr<ProxyEntity>> &out) {
  for (const auto &ent : src) {
    if (!dst.contains(ent))
      out += ent;
  }
}

} // namespace Configs
