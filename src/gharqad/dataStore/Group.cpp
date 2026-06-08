#include <nekobox/dataStore/Group.hpp>
#include <nekobox/dataStore/DatabaseRocksDB.hpp>
#include <QFile>
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/configs/proxy/AbstractBean.hpp>
#include <nekobox/dataStore/Database.hpp>

namespace Configs
{
    bool Group::saveNotes(const QString &notes){
        #ifdef DEBUG_MODE
        qDebug() << "OnSaveNotes";
        #endif
        QString path = ("notes/groups/" + QString::number(this->id) + ".note.txt");
        return WriteFileText(path, notes);
    }

    QString Group::getNotes() const{
        QString str = ("notes/groups/" + QString::number(this->id) + ".note.txt");
        return ReadFileText(str);
    };

    Group::Group() {
    }


    std::map<int, std::weak_ptr<GroupExtra>> weakExtraMap;

    static std::shared_ptr<GroupExtra> getExtra(int id){
        if (id < 0){
            return nullptr;
        }
        std::shared_ptr<GroupExtra> extr = nullptr;
        auto iter = weakExtraMap.find(id);
        if (iter != weakExtraMap.end()){
            extr = iter->second.lock();
        }
        if (extr == nullptr){
            extr = std::make_shared<GroupExtra>();
            extr->id = id;
            extr->save_before_destroy(true);
            extr->Load();
            weakExtraMap.emplace(id, extr);
        }
        return extr;
    }

    std::shared_ptr<const GroupExtra> GetExtra(std::shared_ptr<Group> ent){
        if (ent == nullptr){
            return nullptr;
        }
        return getExtra(ent->id);
    };
    static std::shared_ptr<GroupExtra> getExtraUnlocked(int id){
        auto extr = getExtra(id);
        if (extr == nullptr){
            return nullptr;
        }
        extr->save_before_destroy(true);
        return extr;
    };
    std::shared_ptr<GroupExtra> GetExtraUnlocked(std::shared_ptr<Group> ent){
        return getExtraUnlocked(ent->id);
    };
    GroupExtra::~GroupExtra(){
        weakExtraMap.erase(this->id);
        if (this->save_before_destroy()){
            this->Save();
        }
    }
    
    void GroupExtra::fallback_job(JsonStore * store) {
        if (auto group = dynamic_cast<Configs::Group*>(store)){
            group->is_subscription = !this->url.isEmpty();
        }
        this->Save();
    };

    std::shared_ptr<JsonStore> Group::fallback() {
        return this->getExtraUnlocked();
    };

    #ifndef  d_add
    #define  d_add(X, Y, B) _put(ptr, X, &this->Y, B)
    #endif

    DECL_MAP(Group)
        ADD_MAP("id", id, integer);
        ADD_MAP("front_proxy_id", front_proxy_id, integer);
        ADD_MAP("landing_proxy_id", landing_proxy_id, integer);
        ADD_MAP("archive", archive, boolean);
    //    ADD_MAP("skip_auto_update", skip_auto_update, boolean);
        ADD_MAP("name", name, string);
        ADD_MAP("profiles", profiles, integerList);
        ADD_MAP("is_subscription", is_subscription, boolean);
    //    ADD_MAP("url", url, string);
    //    ADD_MAP("info", info, string);
    //    _add(new configItem("notes", &notes, string));
    //    ADD_MAP("lastup", sub_last_update, integer64);
    //    d_add("manually_column_width", manually_column_width, boolean);
    //    d_add("column_width", column_width, integerList);
    STOP_MAP

    QList<int> Group::Profiles() const {
        return profiles;
    }

    int Group::DropNulls() {
        QList<int> drop;
        for (auto id : profiles){
            auto ent = profileManager->GetProfile(id);
            if (ent == nullptr){
                drop << id;
            }
        }
        for (auto id: drop){
            profiles.removeAll(id);
        }
        this->Save();
        return drop.size();
    }
/*
    QList<std::shared_ptr<ProxyEntity>> Group::GetProfileEnts() const
    {
        auto res = QList<std::shared_ptr<ProxyEntity>>{};
        for (auto id : profiles)
        {
            std::shared_ptr<Configs::ProxyEntity> ent = profileManager->GetProfile(id);
            if (ent) {
                res.append(ent);
            } 
            #ifdef DEBUG_MODE
            else {
                qDebug() << "Found missing profile ID:" << id;
            }
            #endif
        }
        return res;
    }
*/

    bool Group::AddProfile(int id)
    {
        if (HasProfile(id))
        {
            return false;
        }
        profiles.append(id);
        return true;
    }

    bool Group::RemoveProfile(int id)
    {
        if (!HasProfile(id)) return false;
        profiles.removeAll(id);
        return true;
    }

    bool Group::SwapProfiles(int idx1, int idx2)
    {
        if (profiles.size() <= idx1 || profiles.size() <= idx2) return false;
        profiles.swapItemsAt(idx1, idx2);
        return true;
    }

    bool Group::EmplaceProfile(int idx, int newIdx)
    {
        if (profiles.size() <= idx || profiles.size() <= newIdx) return false;
        profiles.insert(newIdx+1, profiles[idx]);
        if (idx < newIdx) profiles.remove(idx);
        else profiles.remove(idx+1);
        return true;
    }

    bool Group::HasProfile(int id) const
    {
        return profiles.contains(id);
    }

    std::shared_ptr<const GroupExtra> Group::getExtra(){
        return Configs::getExtra(this->id);
    };

    std::shared_ptr<GroupExtra> Group::getExtraUnlocked(){
        return Configs::getExtraUnlocked(this->id);
    };
}
