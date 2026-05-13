#ifdef _WIN32
#include <winsock2.h>
#endif



#pragma once

#include <QString>

struct DeviceDetails {
    QString hwid;
    QString os;
    QString osVersion;
    QString model;
};

DeviceDetails GetDeviceDetails();