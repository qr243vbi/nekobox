#ifdef Q_OS_WIN

#pragma once

class QWidget;

void Windows_QWidget_SetForegroundWindow(QWidget* w);

bool Windows_IsInAdmin();

#endif