struct Case { const char *name; const char *header; const char *used; const char *remain; const char *expire; };
const Case cases[] = {
    {"full", "upload=1024; download=1024; total=4096; expire=1700000000", "2.00 KiB", "2.00 KiB", "date"},
    {"no_total", "upload=1024; download=1024; expire=1700000000", "2.00 KiB", "-", "date"},
    {"only_expiry", "expire=1700000000", "-", "-", "date"},
    {"no_expiry", "upload=1024; download=1024; total=4096", "2.00 KiB", "2.00 KiB", "-"},
    {"zero_expiry", "upload=1024; download=1024; total=4096; expire=0", "2.00 KiB", "2.00 KiB", "-"},
    {"zero_total", "upload=1024; download=1024; total=0; expire=1700000000", "2.00 KiB", "∞", "date"},
    {"both_zero", "upload=0; download=0; total=0; expire=0", "0.00 B", "∞", "-"},
    {"only_total", "total=4096", "-", "-", "-"},
    {"only_unlimited", "total=0", "-", "∞", "-"},
    {"only_upload", "upload=1024", "1.00 KiB", "-", "-"},
    {"only_download", "download=1024", "1.00 KiB", "-", "-"},
    {"partial_upload", "upload=1024; total=4096; expire=1700000000", "1.00 KiB", "3.00 KiB", "date"},
    {"partial_download", "download=1024; total=4096; expire=1700000000", "1.00 KiB", "3.00 KiB", "date"},
    {"spaces", "  upload = 1024 ; download\t=\t1024 ; total = 4096 ; expire = 1700000000  ", "2.00 KiB", "2.00 KiB", "date"},
    {"order", "expire=1700000000;total=4096;download=1024;upload=1024", "2.00 KiB", "2.00 KiB", "date"},
    {"empty_fields", ";;upload=1024;;;download=1024;total=4096;;expire=1700000000;;", "2.00 KiB", "2.00 KiB", "date"},
    {"unrelated_fields", "custom=123;upload=1024;download=1024;total=4096;expire=1700000000;x=1", "2.00 KiB", "2.00 KiB", "date"},
    {"over_quota", "upload=3;download=2;total=4;expire=1700000000", "5.00 B", "-1.00 B", "date"},
    {"malformed_total", "upload=1024;download=1024;total=oops;expire=1700000000", "2.00 KiB", "-", "date"},
    {"negative_total", "upload=1024;download=1024;total=-1;expire=1700000000", "2.00 KiB", "-", "date"},
    {"suffix_total", "upload=1024;download=1024;total=4096junk;expire=1700000000", "2.00 KiB", "-", "date"},
    {"prefix_total", "upload=1024;download=1024;xtotal=4096;expire=1700000000", "2.00 KiB", "-", "date"},
    {"overflow_total", "upload=1024;download=1024;total=9223372036854775808;expire=1700000000", "2.00 KiB", "-", "date"},
    {"invalid_upload", "upload=oops;download=1024;total=4096;expire=1700000000", "-", "-", "date"},
    {"negative_download", "upload=1024;download=-1;total=4096;expire=1700000000", "-", "-", "date"},
    {"overflow_upload", "upload=9223372036854775808;download=1024;total=4096;expire=1700000000", "-", "-", "date"},
    {"overflow_sum", "upload=9223372036854775807;download=1;total=4096;expire=1700000000", "-", "-", "date"},
    {"invalid_expiry", "upload=1024;download=1024;total=4096;expire=oops", "2.00 KiB", "2.00 KiB", "-"},
    {"negative_expiry", "upload=1024;download=1024;total=4096;expire=-1", "2.00 KiB", "2.00 KiB", "-"},
    {"overflow_expiry", "upload=1024;download=1024;total=4096;expire=9223372036854775808", "2.00 KiB", "2.00 KiB", "-"},
    {"overflow_milliseconds", "upload=1024;download=1024;total=4096;expire=9223372036854775807", "2.00 KiB", "2.00 KiB", "-"},
    {"empty", "", nullptr, nullptr, nullptr},
    {"whitespace", " \t\r\n", nullptr, nullptr, nullptr},
    {"unknown", "foo=123; bar=456", nullptr, nullptr, nullptr},
    {"all_invalid", "upload=no;download=-1;total=oops;expire=bad", nullptr, nullptr, nullptr},
    {"no_separators", "upload=1024 download=1024 total=4096 expire=1700000000", nullptr, nullptr, nullptr},
    {"comma_separator", "upload=1024,download=1024,total=4096,expire=1700000000", nullptr, nullptr, nullptr},
    {"first_duplicate", "upload=1024;download=1024;total=4096;total=8192;expire=1700000000", "2.00 KiB", "2.00 KiB", "date"},
    {"invalid_first_duplicate", "upload=1024;download=1024;total=bad;total=8192;expire=1700000000", "2.00 KiB", "-", "date"},
};

int main(int argc, char **argv) {
    QLocale::setDefault(QLocale::c());
    if (argc != 2) return 2;
    if (std::string(argv[1]) == "headers") {
        std::cout << "Qt runtime=" << qVersion() << " headers=" << QT_VERSION_STR << '\n';
        const QString metadata = "upload=1024;download=1024;total=4096;expire=1700000000";
        for (const auto *name : {"Subscription-UserInfo", "subscription-userinfo", "SUBSCRIPTION-USERINFO", "SuBsCrIpTiOn-UsErInFo"}) {
            const std::map<std::string, std::string> raw{{name, metadata.toStdString()}, {"Content-Type", "text/plain"}};
            const auto headers = QStdMapString2QMapEnumFieldName(raw);
            const auto value = NetworkRequestHelper::GetHeader(headers, "Subscription-UserInfo");
            if (value != metadata || ParseSubInfo(value) != ParseSubInfo(metadata)) return 1;
        }
        if (!NetworkRequestHelper::GetHeader({}, "Subscription-UserInfo").isEmpty()) return 1;
        std::cout << "PASS header lookup: four case variants and missing header (5 checks)\n";
        return 0;
    }
    const auto &row = cases[std::stoi(argv[1])];
    const QString actual = ParseSubInfo(QString::fromUtf8(row.header));
    QString expected;
    if (row.used) {
        const QString expiry = std::string(row.expire) == "date"
            ? QLocale::c().toString(QDateTime::fromSecsSinceEpoch(1700000000), QLocale::ShortFormat)
            : QString::fromUtf8(row.expire);
        expected = QString("Used: %1 Remain: %2 Expire: %3").arg(
            QString::fromUtf8(row.used), QString::fromUtf8(row.remain), expiry);
    }
    if (actual != expected) {
        std::cout << "FAIL " << row.name << "\n  expected: " << expected.toStdString()
                  << "\n  actual:   " << actual.toStdString() << '\n';
        return 1;
    }
    std::cout << "PASS " << row.name << '\n';
    return 0;
}
