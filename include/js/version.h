#ifndef VERSION_GETTER
#define VERSION_GETTER
#if NKR_VERSION == getNkrVersion

const char * getVersionString();
#define getNkrVersion getVersionString()

#endif
#endif
