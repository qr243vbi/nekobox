// Drives the production LogFile.hpp; the runner checks the file it leaves.
#include <nekobox/global/LogFile.hpp>

#include <QDir>
#include <QThread>
#include <cstdio>
#include <cstring>
#include <vector>

static int previousCalls = 0;
static void previousHandler(QtMsgType, const QMessageLogContext &, const QString &) {
    ++previousCalls;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "";
    if (!std::strcmp(mode, "disabled")) {
        LogFile::append("must not be written");
        std::printf("enabled=%d\n", LogFile::enabled() ? 1 : 0);
        return 0;
    }
    if (!std::strcmp(mode, "lines")) {
        // the path is resolved on first use, a later directory change keeps it
        std::printf("enabled=%d\n", LogFile::enabled() ? 1 : 0);
        QDir::setCurrent(argc > 2 ? argv[2] : ".");
        LogFile::append("first line\n\x1B[33mWARN\x1B[0m coloured\r\n\n   padded   \n");
        LogFile::append("second call");
        return 0;
    }
    if (!std::strcmp(mode, "qt")) {
        qInstallMessageHandler(previousHandler);
        LogFile::installQtMessageHandler();
        qWarning("warned %d", 1);
        qInfo("informed");
        std::printf("previous=%d\n", previousCalls);
        return 0;
    }
    if (!std::strcmp(mode, "threads")) {
        std::vector<QThread *> threads;
        for (int t = 0; t < 8; ++t) {
            threads.push_back(QThread::create([t] {
                for (int i = 0; i < 200; ++i) {
                    LogFile::append(QString("thread %1 line %2 ").arg(t).arg(i) + QString(40, 'x'));
                }
            }));
            threads.back()->start();
        }
        for (auto *thread : threads) {
            thread->wait();
            delete thread;
        }
        return 0;
    }
    if (!std::strcmp(mode, "unwritable")) {
        LogFile::append("cannot be written");
        LogFile::append("still no crash");
        std::printf("enabled=%d\n", LogFile::enabled() ? 1 : 0);
        return 0;
    }
    return 2;
}
