#include "include/global/DeviceDetailsHelper.hpp"

#include <QString>
#include <QSysInfo>
#include <QFile>
#include <vector>   
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#include "include/sys/windows/WinVersion.h"
#include <include/sys/Process.hpp>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#endif


#ifdef Q_OS_WIN
static QString queryWmiProperty(const QString& wmiClass, const QString& property) {
    HRESULT hres;

    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) return QString();

    hres = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL
    );
    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
        CoUninitialize();
        return QString();
    }

    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator, 0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc
    );
    if (FAILED(hres)) {
        CoUninitialize();
        return QString();
    }

    IWbemServices* pSvc = NULL;
    BSTR bstrNamespace = SysAllocString(L"ROOT\\CIMV2");
    hres = pLoc->ConnectServer(
        bstrNamespace,
        NULL, NULL, NULL, 0, NULL, 0, &pSvc
    );
    SysFreeString(bstrNamespace);
    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        return QString();
    }

    hres = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return QString();
    }

    IEnumWbemClassObject* pEnumerator = NULL;
    QString query = QString("SELECT %1 FROM %2").arg(property, wmiClass);
    BSTR bstrWQL = SysAllocString(L"WQL");
    BSTR bstrQuery = SysAllocString(query.toStdWString().c_str());
    hres = pSvc->ExecQuery(
        bstrWQL,
        bstrQuery,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );
    SysFreeString(bstrWQL);
    SysFreeString(bstrQuery);
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return QString();
    }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    QString result;

    if (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (uReturn) {
            VARIANT vtProp;
            VariantInit(&vtProp);
            hr = pclsObj->Get(property.toStdWString().c_str(), 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                result = QString::fromWCharArray(vtProp.bstrVal);
            }
            VariantClear(&vtProp);
            pclsObj->Release();
        }
        pEnumerator->Release();
    }

    pSvc->Release();
    pLoc->Release();
    CoUninitialize();

    return result;
}

static QString winBaseBoard() {
    return queryWmiProperty("Win32_BaseBoard", "Product");
}

static QString winModel() {
    return queryWmiProperty("Win32_ComputerSystem", "Model");
}
#endif

DeviceDetails GetDeviceDetails() {
    DeviceDetails details;

#ifdef Q_OS_WIN
    details.hwid = QSysInfo::machineUniqueId();
    if (details.hwid.isEmpty()) {
        auto productType = QSysInfo::productType().toUtf8();
        details.hwid = QString("%1-%2").arg(QSysInfo::machineHostName(), QString::fromUtf8(productType));
    }

    details.os = QStringLiteral("Windows");
    VersionInfo info;
    WinVersion::GetVersion(info);
    details.osVersion = QString("%1.%2.%3").arg(info.Major).arg(info.Minor).arg(info.BuildNum);
    auto wm = winModel();
    auto wbb = winBaseBoard();
    details.model = (wm == wbb) ? wm : wm + "/" + wbb;
#elif defined(Q_OS_LINUX)
    QString mid;
    QFile f1("/etc/machine-id");
    if (f1.exists() && f1.open(QIODevice::ReadOnly | QIODevice::Text)) {
        mid = QString::fromUtf8(f1.readAll()).trimmed();
        f1.close();
    }
    else {
        QFile f2("/var/lib/dbus/machine-id");
        if (f2.exists() && f2.open(QIODevice::ReadOnly | QIODevice::Text)) {
            mid = QString::fromUtf8(f2.readAll()).trimmed();
            f2.close();
        }
    }
    details.hwid = mid;
    details.os = QStringLiteral("Linux");
    details.osVersion = QSysInfo::kernelVersion();
    details.model = QSysInfo::prettyProductName();
#elif defined(Q_OS_MACOS)
    details.hwid = QSysInfo::machineUniqueId();
    details.os = QStringLiteral("macOS");
    details.osVersion = QSysInfo::productVersion();
    details.model = QSysInfo::prettyProductName();
#else
    details.hwid = QSysInfo::machineUniqueId();
    details.os = QSysInfo::productType();
    details.osVersion = QSysInfo::productVersion();
    details.model = QSysInfo::prettyProductName();
#endif
    return details;
}