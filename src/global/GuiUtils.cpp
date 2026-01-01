#include <nekobox/global/GuiUtils.hpp>



QWidget *GetMessageBoxParent() {
    auto activeWindow = QApplication::activeWindow();
    if (activeWindow == nullptr && mainwindow != nullptr) {
        if (mainwindow->isVisible()) return mainwindow;
        return nullptr;
    }
    return activeWindow;
}
/*
int MessageBoxWarning(const QString &title, const QString &text) {
    return QMessageBox::warning(GetMessageBoxParent(), title, text);
}

int MessageBoxInfo(const QString &title, const QString &text) {
    return QMessageBox::information(GetMessageBoxParent(), title, text);
}
*/
void ActivateWindow(QWidget *w) {
    ToggleWindow(w);
}

void ToggleWindow(QWidget *w) {
    if (w->isVisible() && !(w->windowState() & Qt::WindowMinimized)) {
        // Window is visible → minimize / hide
        w->hide();
    } else {
        // Window is hidden or minimized → show
        w->setWindowState(w->windowState() & ~Qt::WindowMinimized);
        w->setVisible(true);
#ifdef Q_OS_WIN
        Windows_QWidget_SetForegroundWindow(w);
#endif
        w->raise();
        w->activateWindow();
    }
}

void runOnUiThread(const std::function<void()> &callback) {
    // any thread
    auto *timer = new QTimer();
    auto thread = mainwindow->thread();
    timer->moveToThread(thread);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, [=]() {
        // main thread
        callback();
        timer->deleteLater();
    });
    QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));
}


void setTimeout(const std::function<void()> &callback, QObject *obj, int timeout) {
    auto t = new QTimer;
    QObject::connect(t, &QTimer::timeout, obj, [=] {
        callback();
        t->deleteLater();
    });
    t->setSingleShot(true);
    t->setInterval(timeout);
    t->start();
}
