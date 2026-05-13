#ifdef _WIN32
#include <winsock2.h>
#endif




#pragma once

class QWidget;

void Windows_QWidget_SetForegroundWindow(QWidget* w);

bool Windows_IsInAdmin();

