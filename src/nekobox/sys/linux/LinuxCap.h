#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once
#include <string>

bool Unix_HavePkexec();

void Unix_SetCrashHandler();

bool prepare_directory_for_shared_access(const std::string& path);