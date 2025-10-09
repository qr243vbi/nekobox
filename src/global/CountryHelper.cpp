#include "include/global/CountryHelper.hpp"

QString CountryNameToCode(const QString& countryName) {
    return CountryMap.value(countryName, "");
}

QString CountryCodeToFlag(const QString& countryCode) {
    QVector<uint> ucs4 = countryCode.toUcs4();
    for (uint& code : ucs4) {
        code += 0x1F1A5;
    }
    return QString::fromUcs4(ucs4.data(), countryCode.length());
}