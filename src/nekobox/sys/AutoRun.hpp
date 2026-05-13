#ifdef _WIN32
#include <winsock2.h>
#endif



#pragma once

void AutoRun_SetEnabled(bool enable);

bool AutoRun_IsEnabled();
