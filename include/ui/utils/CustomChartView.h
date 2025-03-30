#pragma once

#include <QtCharts/QChartView>
#include <QTimer>

class CustomChartView : public QChartView
{
    Q_OBJECT

public:
    explicit CustomChartView(QChart *chart, QWidget *parent = nullptr)
        : QChartView(chart, parent),
    tooltipTimer(new QTimer(this)),
    lastMousePos(QPoint(-1, -1))
    {
        setMouseTracking(true);
        tooltipTimer->setInterval(200);
        tooltipTimer->setSingleShot(true);
        connect(tooltipTimer, &QTimer::timeout, this, [=]{emit mouseStopEvent(lastMousePos);});
    }

    signals:
    void mouseStopEvent(QPoint pos);

    void mouseStartMoving();

protected:
    void mouseMoveEvent(QMouseEvent *event) override
    {
        lastMousePos = event->pos();
        event->ignore();
        emit mouseStartMoving();
        tooltipTimer->start();
    }

private:
    QTimer *tooltipTimer;
    QPoint lastMousePos;
};