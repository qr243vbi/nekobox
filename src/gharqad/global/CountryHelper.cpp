#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/global/CountryHelper.hpp>

QString CountryNameToCode(const QString& countryName) {
    return CountryMap.value(countryName, "");
}

QString CountryCodeToFlag(const QString& countryCode) {
    QVector<uint> ucs4 = countryCode.toUcs4();
    for (uint& code : ucs4) {
        code += 0x1F1A5;
    }
    return QString::fromUcs4((char32_t*)ucs4.data(), countryCode.length());
}


QString CountryNameToCode(const std::string& countryName) {
    return CountryNameToCode(QString::fromUtf8(countryName.c_str()));
}


QString CountryCodeToFlag(const std::string& countryName) {
    return CountryNameToCode(QString::fromUtf8(countryName.c_str()));
}
