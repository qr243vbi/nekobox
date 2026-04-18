#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/sys/linux/LinuxCap.h>
#include <QDebug>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <sys/resource.h>


#include <sys/stat.h>
#include <sys/types.h>
#include <sys/acl.h>
#include <grp.h>
#include <unistd.h>
#include <string>
#include <cstdio>

bool prepare_directory_for_shared_access(const std::string& path)
{
    uid_t uid = geteuid();

    if (chmod(path.c_str(), 02770) == -1) {
        perror("chmod failed");
        return false;
    }

    acl_t acl = acl_init(5);
    if (!acl) return false;

    acl_entry_t e;
    acl_permset_t p;

    // --- USER_OBJ (owner) ---
    acl_create_entry(&acl, &e);
    acl_set_tag_type(e, ACL_USER_OBJ);
    acl_get_permset(e, &p);
    acl_add_perm(p, ACL_READ | ACL_WRITE | ACL_EXECUTE);

    // --- USER (your process) ---
    acl_create_entry(&acl, &e);
    acl_set_tag_type(e, ACL_USER);
    acl_set_qualifier(e, &uid);
    acl_get_permset(e, &p);
    acl_add_perm(p, ACL_READ | ACL_WRITE | ACL_EXECUTE);

    // --- GROUP_OBJ ---
    acl_create_entry(&acl, &e);
    acl_set_tag_type(e, ACL_GROUP_OBJ);
    acl_get_permset(e, &p);
    acl_add_perm(p, ACL_READ | ACL_WRITE | ACL_EXECUTE);

    // --- OTHER ---
    acl_create_entry(&acl, &e);
    acl_set_tag_type(e, ACL_OTHER);
    acl_get_permset(e, &p);
    acl_clear_perms(p);

    // --- MASK ---
    acl_create_entry(&acl, &e);
    acl_set_tag_type(e, ACL_MASK);
    acl_get_permset(e, &p);
    acl_add_perm(p, ACL_READ | ACL_WRITE | ACL_EXECUTE);

    // Apply
    if (acl_set_file(path.c_str(), ACL_TYPE_ACCESS, acl) == -1) {
        perror("acl_set_file ACCESS");
        acl_free(acl);
        return false;
    }

    if (acl_set_file(path.c_str(), ACL_TYPE_DEFAULT, acl) == -1) {
        perror("acl_set_file DEFAULT");
        acl_free(acl);
        return false;
    }

    acl_free(acl);
    return true;
}


void Unix_SetCrashHandler() {
    rlimit rl;
    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &rl);
}

bool Unix_HavePkexec() {
    QProcess p;
    p.setProgram("pkexec");
    p.setArguments({"--help"});
    p.setProcessChannelMode(QProcess::SeparateChannels);
    p.start();
    p.waitForFinished(500);
    return (p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1) == 0;
}
