#include <nekobox/global/GuiUtils.hpp>
#include <qnamespace.h>

QWidget *GetMessageBoxParent() {
  auto activeWindow = QApplication::activeWindow();
  if (activeWindow == nullptr && mainwindow != nullptr) {
    if (mainwindow->isVisible())
      return mainwindow;
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
void ActivateWindow(QWidget *w) { ToggleWindow(w); }

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
  QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection,
                            Q_ARG(int, 0));
}

void setTimeout(const std::function<void()> &callback, QObject *obj,
                int timeout) {
  auto t = new QTimer;
  QObject::connect(t, &QTimer::timeout, obj, [=] {
    callback();
    t->deleteLater();
  });
  t->setSingleShot(true);
  t->setInterval(timeout);
  t->start();
}

std::list<ProxyColorRule> latencyColorList = {
    {1, 5, 0, 0, false, Qt::darkGreen},
    {6, 5, 0, 0, false, QColor(128, 0, 128)},
    {0, 0, 0, 0, false, QColor(255, 165, 0)},
    {0, 0, 0, 0, true, Qt::red}};


QColor DisplayLatencyColor(Configs::ProxyEntity *e) {
  if (e != nullptr) {
    if (e->latencyInt < 0) {
      for (auto &color : latencyColorList) {
        if (color.unavailable) {
          return color.color;
        }
      }
    } else {
      for (auto &color : latencyColorList) {
        if (!color.unavailable) {
          if (color.orderRange > 0) {
            if (e->latencyOrder < (int)color.orderMin) {
              continue;
            }
            if (e->latencyOrder > (int)(color.orderMin + color.orderRange)) {
              continue;
            }
          }
          if (color.latencyRange > 0) {
            if (e->latencyInt < (int)color.latencyMin) {
              continue;
            }
            if (e->latencyInt > (int)(color.latencyMin + color.latencyRange)) {
              continue;
            }
          }
          return color.color;
        }
      }
    }
  }
  return Qt::black;
}

std::map<Icon::TrayIconStatus, IndicatorRule> indicatorRuleMap = {
    {Icon::TrayIconStatus::VPN, {0.4, 0.04, 0.4, QColor(165, 42, 42)}},
    {Icon::TrayIconStatus::DNS, {0.4, 0.04, 0.4, Qt::darkMagenta}},
    {Icon::TrayIconStatus::SYSTEM_PROXY, {0.4, 0.04, 0.4, Qt::blue}},
    {Icon::TrayIconStatus::SYSTEM_PROXY_DNS, {0.4, 0.04, 0.4, Qt::darkMagenta}},
    {Icon::TrayIconStatus::RUNNING, {0.4, 0.04, 0.4, Qt::darkGreen}}
};
