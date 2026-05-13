#ifdef _WIN32
#include <winsock2.h>
#endif

#pragma once

#include <QDialog>
#include <QLabel>
#include <QTimer>
#include <QRect>

class ShutdownOverlay : public QDialog {
    Q_OBJECT

public:
    explicit ShutdownOverlay(QWidget *parent, bool exit = true);

    void start();
    void finish();

private:
    QLabel *label;
    QLabel *picture;
    QTimer timer;
    int dots = 0;
};