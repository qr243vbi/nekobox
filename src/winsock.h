

#ifdef _WIN32
#ifndef _NEKOBOX_WINSOCKS2_COMPATIBILITY_
#define _NEKOBOX_WINSOCKS2_COMPATIBILITY_

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

#endif
#endif
