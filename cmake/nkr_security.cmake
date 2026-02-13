
set(SECURITY_SOURCES
        ${NEKOBOX_INCLUDE}/ui/security/security.ui
        ${NEKOBOX_INCLUDE}/ui/security/change_password.ui
        ${NEKOBOX_INCLUDE}/ui/security/confirm_password.ui
        ${NEKOBOX_INCLUDE}/ui/security/security.h
        ${NEKOBOX_INCLUDE}/ui/security/delete_or_edit_users.ui
        src/ui/security_addon.cpp
)
    nkr_add_compile_definitions(NKR_SOFTWARE_KEYS=\"${NKR_SOFTWARE_KEYS}\")