
set(SECURITY_SOURCES
        ${NYXARIA}/ui/security/security.ui
        ${NYXARIA}/ui/security/change_password.ui
        ${NYXARIA}/ui/security/confirm_password.ui
        ${NYXARIA}/ui/security/security.h
        ${NYXARIA}/ui/security/delete_or_edit_users.ui
        ${GHARQAD}/ui/security_addon.cpp
)
    nkr_add_compile_definitions(NKR_SOFTWARE_KEYS=\"${NKR_SOFTWARE_KEYS}\")
