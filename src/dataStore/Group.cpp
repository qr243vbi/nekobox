#include <include/dataStore/Group.hpp>
#include <QFile>
#include "include/ui/profile/dialog_edit_profile.h"
#include "include/global/Utils.hpp"
#include "include/configs/proxy/AbstractBean.hpp"


namespace Configs
{
    bool Group::saveNotes(const QString &notes){
        QString path = ("groups/" + QString::number(this->id) + ".note.txt");
        return WriteFileText(path, notes);
    }

    QString Group::getNotes() const{
        QString str = ("groups/" + QString::number(this->id) + ".note.txt");
        return ReadFileText(str);
    };

    Group::Group() {
    }

    #ifndef  d_add
    #define  d_add(X, Y, B) _put(ptr, X, &this->Y, B)
    #endif

    DECL_MAP(Group)
        ADD_MAP("id", id, integer);
        ADD_MAP("front_proxy_id", front_proxy_id, integer);
        ADD_MAP("landing_proxy_id", landing_proxy_id, integer);
        ADD_MAP("archive", archive, boolean);
        ADD_MAP("skip_auto_update", skip_auto_update, boolean);
        ADD_MAP("name", name, string);
        ADD_MAP("profiles", profiles, integerList);
        ADD_MAP("url", url, string);
        ADD_MAP("info", info, string);
    //    _add(new configItem("notes", &notes, string));
        ADD_MAP("lastup", sub_last_update, integer64);
    //    d_add("manually_column_width", manually_column_width, boolean);
    //    d_add("column_width", column_width, integerList);
    STOP_MAP

    QList<int> Group::Profiles() const {
        return profiles;
    }

    QList<std::shared_ptr<ProxyEntity>> Group::GetProfileEnts() const
    {
        auto res = QList<std::shared_ptr<ProxyEntity>>{};
        for (auto id : profiles)
        {
            res.append(profileManager->GetProfile(id));
        }
        return res;
    }


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
}
