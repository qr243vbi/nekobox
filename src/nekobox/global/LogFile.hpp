#pragma once

// Optional mirror of the application log into a file.
//
// Enabled by setting the NEKOBOX_LOG_FILE environment variable to a file path
// before starting the program. Every line that goes to the log tab (GUI
// messages and nekobox_core output) and every Qt message is appended to that
// file with a timestamp. Without the variable nothing is written.

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QString>
#include <QStringList>

#include <nekobox/dataStore/Utils.hpp>

namespace LogFile {

inline QString &path() {
  static QString value = [] {
    const QString env = qEnvironmentVariable("NEKOBOX_LOG_FILE");
    // Resolve once, before the program changes its working directory.
    return env.isEmpty() ? QString() : QFileInfo(env).absoluteFilePath();
  }();
  return value;
}

inline bool enabled() { return !path().isEmpty(); }

inline void append(const QString &text) {
  if (!enabled()) {
    return;
  }
  // A Qt message raised while writing comes back here through the message
  // handler; drop it instead of locking the mutex a second time.
  static thread_local bool busy = false;
  if (busy) {
    return;
  }
  busy = true;
  static QMutex mutex;
  {
    QMutexLocker lock(&mutex);
    static QFile file(path());
    if (file.isOpen() || file.open(QIODevice::Append | QIODevice::Text)) {
      const QString stamp =
          QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz ");
      QString out;
      // the same cleanup as the log tab: no colour codes from the core
      for (const QString &line : sanitizeLog(text).split('\n')) {
        if (!line.trimmed().isEmpty()) {
          out += stamp + line.trimmed() + '\n';
        }
      }
      file.write(out.toUtf8());
      file.flush();
    }
  }
  busy = false;
}

// Installs a Qt message handler that mirrors qDebug/qWarning/... into the file
// and keeps the previous handler's behaviour.
inline void installQtMessageHandler() {
  if (!enabled()) {
    return;
  }
  static QtMessageHandler previous = nullptr;
  previous = qInstallMessageHandler(
      [](QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        const char *name = "[Qt] ";
        switch (type) {
        case QtDebugMsg: name = "[Qt debug] "; break;
        case QtInfoMsg: name = "[Qt info] "; break;
        case QtWarningMsg: name = "[Qt warning] "; break;
        case QtCriticalMsg: name = "[Qt critical] "; break;
        case QtFatalMsg: name = "[Qt fatal] "; break;
        }
        append(QString::fromLatin1(name) + msg);
        if (previous) {
          previous(type, context, msg);
        }
      });
}

} // namespace LogFile
