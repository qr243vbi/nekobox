#pragma once

#include <QString>

struct DeviceDetails {
    QString hwid;
    QString os;
    QString osVersion;
    QString model;
};

DeviceDetails GetDeviceDetails();