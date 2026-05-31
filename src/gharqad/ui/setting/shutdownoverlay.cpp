#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/ui/setting/Icon.hpp>
#include <nekobox/ui/setting/shutdownoverlay.hpp>
#include <QVBoxLayout>
#include <QGuiApplication>
#include <QRect>
#include <QScreen>


static QRect randomRect()
{
    QRect screen = QGuiApplication::primaryScreen()->availableGeometry();

    int x = screen.center().x() - 150;
    int y = screen.center().y() - 150;

    return QRect(x, y, 300, 300);
}

ShutdownOverlay::ShutdownOverlay(QWidget *parent, bool exit)
    : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setModal(true);
    
    setGeometry(randomRect());
    QString str;
    if (exit){
        str = tr("Exiting %1").arg(software_name);
    } else {
        str = tr("Starting %1").arg(software_name);
    }

    label = new QLabel(str, this);
    picture = new QLabel("", this);
    picture->setPixmap(Icon::GetTrayIcon());

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(picture, 0, Qt::AlignCenter);
    layout->addWidget(label, 0, Qt::AlignCenter);

    connect(&timer, &QTimer::timeout, this, [str, this]() {
        dots = (dots + 1) % 4;
        label->setText(str + QString(dots, '.'));
    });
}

void ShutdownOverlay::start() {
    show();
    timer.start(300);
}

void ShutdownOverlay::finish() {
    timer.stop();
    accept();   // closes dialog cleanly
}