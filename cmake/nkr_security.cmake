
set(SECURITY_SOURCES
        ${NEKOBOX_INCLUDE}/ui/security/security.ui
        ${NEKOBOX_INCLUDE}/ui/security/change_password.ui
        ${NEKOBOX_INCLUDE}/ui/security/security.h
)
    nkr_add_compile_definitions(NKR_SOFTWARE_KEYS=\"${NKR_SOFTWARE_KEYS}\")