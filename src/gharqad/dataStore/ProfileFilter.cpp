#ifdef _WIN32
#include <winsock2.h>
#endif
#include <nekobox/dataStore/ProfileFilter.hpp>

namespace Configs_ConfigItem {
  signed char JsonStore::compare(JsonStore * store, const QList<QString> &skip){
    if (store == nullptr){
      return 1;
    }
    auto &_map1 = this->_map();
    auto &_map2 = store->_map();
    auto keys = _map1.keys();

    for (auto key: keys){
      auto item1 = _map1.find(key);
      auto item2 = _map2.find(key);
      if (item2 == _map2.end()) {
        return 1;
      }
      auto item1val = item1.value();
      auto item2val = item2.value();
      if (item1val != nullptr){
        if (skip.contains(item1val->name)){
          continue;
        }
      }
      if (item1val == nullptr){
        if (item2val == nullptr){
          continue;
        }
        return 1;
      } else if (item2val == nullptr){
        return -1;
      }
      auto ret = item1val->compare(this, item2val.get(), store);
      if (ret != 0){
        return ret;
      }
    }
    return 0;
  }
}

namespace Configs {

// --- Equality ---
bool ProfileFilterKey::operator==(const ProfileFilterKey &other) const noexcept
{
  bool this_key_nullptr = key == nullptr;
  bool other_key_nullptr = other.key == nullptr;
  if (this_key_nullptr){
    return other_key_nullptr;
  }
    return this_key_nullptr == other_key_nullptr
        && key->type == other.key->type
        && key->serverAddress == other.key->serverAddress
        && key->serverPort == other.key->serverPort
        && this->unpack_bean == other.unpack_bean
        && (this->unpack_bean || key->compare(other.key.get(), {"c_cfg", "c_out"}) == 0);
}

bool ProfileFilterKey::operator!=(const ProfileFilterKey &other) const noexcept
{
    return !(*this == other);
}

// --- Ordering (STRICT WEAK ORDERING) ---
bool ProfileFilterKey::operator<(const ProfileFilterKey &other) const noexcept
{
  bool this_key_nullptr = key == nullptr;
  bool other_key_nullptr = other.key == nullptr;
  if (other_key_nullptr){
    return false;
  }
    return (this_key_nullptr)
        || key->type < other.key->type
        || key->serverAddress < other.key->serverAddress
        || key->serverPort < other.key->serverPort
        || ( (!this->unpack_bean) && other.unpack_bean )
        || ( (!this->unpack_bean) && key->compare(other.key.get(), {"c_cfg", "c_out"}) < 0 );
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


// --- Helper factory ---
ProfileFilterKey ProfileFilter_ent_key(
    const std::shared_ptr<Configs::ProxyEntity> &ent,
    bool by_address)
{
    const bool useAddressOnly = by_address && ent->type != "custom";
    return ProfileFilterKey(ent, !useAddressOnly);
}

    ProfileFilterKey::ProfileFilterKey(const std::shared_ptr<Configs::ProxyEntity>& key,
                     bool unpack_bean) noexcept
    {
      this->key = key;
      this->unpack_bean = unpack_bean;
    }

void ProfileFilter::Uniq(const QList<std::shared_ptr<ProxyEntity>> &in,
                         QList<std::shared_ptr<ProxyEntity>> &out,
                         bool by_address, bool keep_last) {
  std::map<ProfileFilterKey, std::shared_ptr<ProxyEntity>> hashMap;

  for (const auto &ent : in) {
    ProfileFilterKey key = ProfileFilter_ent_key(ent, by_address);
    auto iter = hashMap.find(key);
    auto found = iter != hashMap.end();
    if (found && keep_last) {
      out.removeAll(iter->second);
      found = false;
    } 
    if (!found) {
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
  std::map<ProfileFilterKey, std::shared_ptr<ProxyEntity>> map;

  for (const auto &ent : src) {
    map.insert({ProfileFilter_ent_key(ent, by_address), ent});
  }

  for (const auto &ent : dst) {
    const ProfileFilterKey key = ProfileFilter_ent_key(ent, by_address);
    auto iter = map.find(key);
    if (iter != map.end()) {
      outDst += ent;
      outSrc += iter->second;
    }
  }
}

void ProfileFilter::OnlyInSrc(const QList<std::shared_ptr<ProxyEntity>> &src,
                              const QList<std::shared_ptr<ProxyEntity>> &dst,
                              QList<std::shared_ptr<ProxyEntity>> &out,
                              bool by_address) {
  std::set<ProfileFilterKey> keys;

  for (const auto &ent : src) {
    auto key = ProfileFilter_ent_key(ent, by_address);
    if (!keys.contains(key)) {
      out += ent;
      keys.insert(key);
    }
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


void ProfileFilter::OnlyInSrc_ByIds(
    const QList<std::shared_ptr<ProxyEntity>> &src,
    const QList<std::shared_ptr<ProxyEntity>> &dst,
    QList<std::shared_ptr<ProxyEntity>> &out) {
  std::set<int> ids;
  for (const auto &ent : src) {
    int id;
    if (!ids.contains(id = ent->Id())) {
      out += ent;
      ids.insert(id);
    }
  }
}

} // namespace Configs

