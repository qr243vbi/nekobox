

#pragma once

#include <QMessageBox>
#include <QPointer>
#include <QTimer>

class MessageBoxTimer : public QTimer {
public:
    // QPointer, not a raw pointer: the caller owns the box and deleteLater()s it,
    // and a queued timeoutFunc can still arrive after that.
    QPointer<QMessageBox> msgbox;
    bool showed = false;

    explicit MessageBoxTimer(QObject *parent, QMessageBox *msgbox, int delayMs) : QTimer(parent) {
        connect(this, &QTimer::timeout, this, &MessageBoxTimer::timeoutFunc, Qt::ConnectionType::QueuedConnection);
        this->msgbox = msgbox;
        setSingleShot(true);
        setInterval(delayMs);
        start();
    };

    void cancel() {
        QTimer::stop();
        if (msgbox && showed) {
            msgbox->reject(); // return the timeoutFunc
        }
        // Detach from the box. The timeout may already be sitting in the event
        // queue and disconnect() does not remove a posted QMetaCallEvent, so
        // this object must stay alive (see the callers) and this flag must make
        // a late timeoutFunc a no-op.
        msgbox = nullptr;
    };

private:
    void timeoutFunc() {
        if (!msgbox) return;   // also false when the box was destroyed meanwhile
        showed = true;
        msgbox->exec();
        msgbox = nullptr;
    }
};
