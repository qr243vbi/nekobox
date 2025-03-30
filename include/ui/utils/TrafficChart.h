#pragma once
#include <set>
#include <QSplineSeries>
#include <QDateTimeAxis>
#include <QDateTime>
#include <QValueAxis>
#include <QToolTip>
#include <QLegendMarker>
#include <QStyleHints>

#include "CustomChartView.h"

class TrafficChart
{
private:
    QChart *chart;
    CustomChartView *chartView;

    QVector<int> proxyDlRaw;
    QVector<int> proxyUlRaw;
    QVector<int> directDlRaw;
    QVector<int> directUlRaw;

    QSplineSeries *proxyDlLine;
    QSplineSeries *proxyUpLine;
    QSplineSeries *directDlLine;
    QSplineSeries *directUpLine;
    std::multiset<int> dataSet;

    QDateTimeAxis *timeAxis;
    QValueAxis *valueAxis;

    QMutex tooltipMu;

    int scaleFactor = 1;
    int intervalRange = 90;

    void updateMagnitude()
    {
        auto currMax = dataSet.rbegin().operator*();
        if (currMax == 0) return;
        auto prevScale = scaleFactor;
        // calc new magnitude
        if (currMax <= 1000)
        {
            scaleFactor = 1;
        }
        else if (currMax <= 1000000)
        {
            scaleFactor = 1000;
        }
        else if (currMax <= 1000000000)
        {
            scaleFactor = 1000000;
        }
        else
        {
            scaleFactor = 1000000000;
        }
        valueAxis->setMax(static_cast<double>(currMax) / static_cast<double>(scaleFactor) * 1.1);
        if (prevScale == scaleFactor) return;
        valueAxis->setLabelFormat(getRateLabel());
        scaleData();
    }

    QString getRateLabel() const
    {
        if (scaleFactor == 1) return "%.0f B/s";
        if (scaleFactor == 1000) return "%.2f KB/s";
        if (scaleFactor == 1000000) return "%.2f MB/s";
        if (scaleFactor == 1000000000) return "%.2f GB/s";
        return "%.0f ?/s";
    }

    void scaleData() const
    {
        auto data = proxyDlLine->points();
        for (int i = 0; i < data.size(); i++)
        {
            data[i] = {data[i].x(), static_cast<double>(proxyDlRaw[i]) / scaleFactor};
        }
        proxyDlLine->replace(data);

        data = proxyUpLine->points();
        for (int i = 0; i < data.size(); i++)
        {
            data[i] = {data[i].x(), static_cast<double>(proxyUlRaw[i]) / scaleFactor};
        }
        proxyUpLine->replace(data);

        data = directDlLine->points();
        for (int i = 0; i < data.size(); i++)
        {
            data[i] = {data[i].x(), static_cast<double>(directDlRaw[i]) / scaleFactor};
        }
        directDlLine->replace(data);

        data = directUpLine->points();
        for (int i = 0; i < data.size(); i++)
        {
            data[i] = {data[i].x(), static_cast<double>(directUlRaw[i]) / scaleFactor};
        }
        directUpLine->replace(data);
    }

public:
    explicit TrafficChart()
    {
        // init
        chart = new QChart;
        updateTheme();
        chart->setTitle("Traffic Rate");
        chart->legend()->setVisible(true);
        chart->legend()->setAlignment(Qt::AlignBottom);
        chart->setMargins(QMargins(0, 0, 0, 0));

        proxyDlLine = new QSplineSeries;
        proxyDlLine->setName("Proxy Dl");
        proxyDlLine->setColor(Qt::darkBlue);
        auto pen = proxyDlLine->pen();
        pen.setWidth(2);
        proxyDlLine->setPen(pen);
        chart->addSeries(proxyDlLine);

        proxyUpLine = new QSplineSeries;
        proxyUpLine->setName("Proxy Ul");
        proxyUpLine->setColor(Qt::darkRed);
        pen = proxyUpLine->pen();
        pen.setWidth(2);
        proxyUpLine->setPen(pen);
        chart->addSeries(proxyUpLine);

        directDlLine = new QSplineSeries;
        directDlLine->setName("Direct Dl");
        directDlLine->setColor(Qt::darkGreen);
        pen = directDlLine->pen();
        pen.setWidth(2);
        directDlLine->setPen(pen);
        chart->addSeries(directDlLine);

        directUpLine = new QSplineSeries;
        directUpLine->setName("Direct Ul");
        directUpLine->setColor(Qt::darkYellow);
        pen = directUpLine->pen();
        pen.setWidth(2);
        directUpLine->setPen(pen);
        chart->addSeries(directUpLine);

        timeAxis = new QDateTimeAxis;
        timeAxis->setFormat("hh:mm:ss");
        timeAxis->setTickCount(10);
        chart->addAxis(timeAxis, Qt::AlignBottom);
        proxyDlLine->attachAxis(timeAxis);
        proxyUpLine->attachAxis(timeAxis);
        directDlLine->attachAxis(timeAxis);
        directUpLine->attachAxis(timeAxis);

        valueAxis = new QValueAxis;
        valueAxis->setLabelFormat("%.0f B/s");
        valueAxis->setMin(0);
        valueAxis->setMax(1000);
        chart->addAxis(valueAxis, Qt::AlignLeft);
        proxyDlLine->attachAxis(valueAxis);
        proxyUpLine->attachAxis(valueAxis);
        directDlLine->attachAxis(valueAxis);
        directUpLine->attachAxis(valueAxis);

        // initial values
        auto now = QDateTime::currentDateTime();
        timeAxis->setRange(now.addSecs(-intervalRange + 1), now);
        for (int i = 0; i < intervalRange; ++i)
        {
            proxyDlLine->append(now.addSecs(-intervalRange+i+1).toMSecsSinceEpoch(), 0);
            proxyUpLine->append(now.addSecs(-intervalRange+i+1).toMSecsSinceEpoch(), 0);
            directDlLine->append(now.addSecs(-intervalRange+i+1).toMSecsSinceEpoch(), 0);
            directUpLine->append(now.addSecs(-intervalRange+i+1).toMSecsSinceEpoch(), 0);

            proxyDlRaw << 0;
            proxyUlRaw << 0;
            directDlRaw << 0;
            directUlRaw << 0;

            dataSet.insert(0);
            dataSet.insert(0);
            dataSet.insert(0);
            dataSet.insert(0);
        }

        chartView = new CustomChartView(chart);
        chartView->setRenderHint(QPainter::Antialiasing);

        QObject::connect(chartView, &CustomChartView::mouseStopEvent, chartView, [=](const QPoint pos)
        {
            if (!chartView->rect().contains(chartView->mapFromGlobal(QCursor::pos()))) return;
            auto x = chart->mapToValue(pos).x();
            int idx = -1;
            int mn=5000000;
            for (int i=0;i<proxyDlLine->count();i++)
            {
                if (auto dif = abs(proxyDlLine->at(i).x()-x); dif <= 500000)
                {
                    if (dif < mn)
                    {
                        mn = dif;
                        idx = i;
                    }
                    else if (idx > 0) break;
                }
            }
            if (idx == -1) return;
            const auto format = getRateLabel();
            const auto data = QString::asprintf(
            QString("Proxy Dl: " + format + "\nProxy Ul: " + format + "\nDirect Dl: " + format + "\nDirect Ul: " + format).toStdString().c_str(),
                proxyDlLine->at(idx).y(), proxyUpLine->at(idx).y(), directDlLine->at(idx).y(), directUpLine->at(idx).y());
            QToolTip::showText(chartView->mapToGlobal(pos), data);
        });

        QObject::connect(chartView, &CustomChartView::mouseStartMoving, chartView, [=]
        {
            if (QToolTip::isVisible())
            {
                QToolTip::hideText();
            }
        });

        for (QLegendMarker* marker : chart->legend()->markers())
        {
            QObject::connect(marker, &QLegendMarker::clicked, chartView, [=]
            {
                auto series = static_cast<QLineSeries*>(marker->series());
                double alpha;
                if (series->isVisible())
                {
                    series->hide();
                    marker->setVisible(true);
                    alpha = 0.5;
                } else
                {
                    series->show();
                    alpha = 1.0;
                }
                QBrush brush = marker->labelBrush();
                QColor color = brush.color();
                color.setAlphaF(alpha);
                brush.setColor(color);
                marker->setLabelBrush(brush);

                brush = marker->brush();
                color = brush.color();
                color.setAlphaF(alpha);
                brush.setColor(color);
                marker->setBrush(brush);

                QPen markerPen = marker->pen();
                color = markerPen.color();
                color.setAlphaF(alpha);
                markerPen.setColor(color);
                marker->setPen(markerPen);
            });
        }
    };

    [[nodiscard]] QChartView* getChartView() const
    {
        return chartView;
    }

    void updateChart(int pDl, int pUl, int dDl, int dUl)
    {
        auto now = QDateTime::currentDateTime();
        dataSet.insert(pDl);
        dataSet.insert(pUl);
        dataSet.insert(dDl);
        dataSet.insert(dUl);

        dataSet.erase(dataSet.find(proxyDlRaw.first()));
        dataSet.erase(dataSet.find(proxyUlRaw.first()));
        dataSet.erase(dataSet.find(directDlRaw.first()));
        dataSet.erase(dataSet.find(directUlRaw.first()));

        proxyDlLine->remove(0);
        proxyUpLine->remove(0);
        directDlLine->remove(0);
        directUpLine->remove(0);

        proxyDlRaw.removeFirst();
        proxyUlRaw.removeFirst();
        directDlRaw.removeFirst();
        directUlRaw.removeFirst();

        proxyDlLine->append(now.toMSecsSinceEpoch(), static_cast<double>(pDl) / static_cast<double>(scaleFactor));
        proxyUpLine->append(now.toMSecsSinceEpoch(), static_cast<double>(pUl) / static_cast<double>(scaleFactor));
        directDlLine->append(now.toMSecsSinceEpoch(), static_cast<double>(dDl) / static_cast<double>(scaleFactor));
        directUpLine->append(now.toMSecsSinceEpoch(), static_cast<double>(dUl) / static_cast<double>(scaleFactor));

        proxyDlRaw << pDl;
        proxyUlRaw << pUl;
        directDlRaw << dDl;
        directUlRaw << dUl;

        timeAxis->setRange(now.addSecs(-intervalRange + 1), now);

        updateMagnitude();
    }

    void updateTheme()
    {
        chart->setTheme(qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    }
};
