#include <nekobox/ui/MainWindowState.hpp>
#include "ui_mainwindow.h"
#include <QApplication>
#include <QGraphicsView>
#include <QLabel>
#include <QMouseEvent>
#include <QSettings>
#include <QScrollBar>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

static void check(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
static void events() { QApplication::processEvents(); }

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    try {
        check(MainWindowState::displayName({}, {}) == "NekoBox", "Default name");
        check(MainWindowState::displayName({}, "Iblis") == "NekoBox", "Legacy global name");
        check(MainWindowState::displayName(" NyameBox ") == "NekoBox", "Legacy configured name");
        check(MainWindowState::displayName("nekobox") == "NekoBox", "Canonical capitalization");
        check(MainWindowState::displayName("My personal client", "Iblis") == "My personal client", "Custom name lost");
        std::cout << "PASS: canonical branding and preserved custom names\n";

        QMainWindow window;
        Ui::MainWindow ui;
        ui.setupUi(&window);
        // Icon animation is the only custom UI boundary. Generated layout is
        // production mainwindow.ui; model the hidden Routes page's real size constraint.
        auto *largeHiddenPage = new QLabel("Two route tables", ui.routes_tab);
        largeHiddenPage->setMinimumHeight(300);
        ui.routes_tab->layout()->addWidget(largeHiddenPage);
        ui.graph_tab->layout()->addWidget(new QGraphicsView);
        ui.stats_widget->setCurrentWidget(ui.Logs);
        window.resize(900, 680);
        window.show(); events();
        ui.splitter->setSizes({1000, 64}); events();
        const int oldMinimum = ui.stats_widget->height();
        check(oldMinimum >= 300, "Fixture did not reproduce hidden-page minimum");
        MainWindowState::restoreSplitter(ui.splitter, {});
        ui.splitter->setSizes({1000, 64}); events();
        check(ui.stats_widget->height() <= 100, "Visible log cannot shrink");
        check(ui.stats_widget->height() >= 64, "Log panel disappeared");
        check(ui.splitter->handle(1)->height() >= 8, "Splitter handle too small");
        check(ui.splitter->handle(1)->cursor().shape() == Qt::SplitVCursor, "Splitter resize cursor missing");
        auto saved = ui.splitter->saveState();
        ui.splitter->setSizes({200, 350}); events();
        MainWindowState::restoreSplitter(ui.splitter, saved); events();
        check(ui.stats_widget->height() <= 100, "Panel height not restored");
        ui.routes_tab->layout()->removeWidget(largeHiddenPage);
        auto *routes = MainWindowState::scrollPanel(largeHiddenPage, ui.routes_tab);
        ui.routes_tab->layout()->addWidget(routes);
        ui.stats_widget->setCurrentWidget(ui.routes_tab); events();
        check(routes->verticalScrollBar()->maximum() > 0, "Compact routes page clips its controls without scrolling");
        routes->verticalScrollBar()->setValue(routes->verticalScrollBar()->maximum());
        check(routes->widget()->y() < 0, "Compact routes page cannot scroll to its lower controls");
        ui.stats_widget->setCurrentWidget(ui.Logs); events();
        // Drag the actual splitter handle, including press/move/release.
        auto *handle = ui.splitter->handle(1);
        const int beforeDrag = ui.stats_widget->height();
        const QPoint pressPoint = handle->rect().center();
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(pressPoint), QPointF(handle->mapToGlobal(pressPoint)),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(handle, &press);
        const QPoint local = handle->rect().center() - QPoint(0, 90);
        QMouseEvent move(QEvent::MouseMove, QPointF(local), QPointF(handle->mapToGlobal(local)),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(handle, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(local), QPointF(handle->mapToGlobal(local)),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(handle, &release);
        events();
        check(ui.stats_widget->height() > beforeDrag + 40, "Dragging splitter does not resize panels");
        // Persisted old state must not restore a one-pixel invisible handle.
        ui.splitter->setHandleWidth(1);
        const auto narrowState = ui.splitter->saveState();
        MainWindowState::restoreSplitter(ui.splitter, narrowState); events();
        check(ui.splitter->handleWidth() == 8, "Legacy handle width retained");
        app.setStyleSheet("QSplitter::handle:vertical { height: 1px; }"); events();
        check(ui.splitter->handle(1)->height() >= 8, "Theme shrank splitter handle");
        app.setStyleSheet({});
        MainWindowState::restoreSplitter(ui.splitter, "invalid state"); events();
        check(ui.splitter->sizes()[0] > 64 && ui.splitter->sizes()[1] > 64, "Corrupt state has no usable fallback");
        std::cout << "PASS: real UI log resizing, mouse drag, saved height and old/corrupt state\n";

        QTemporaryDir directory;
        QSettings settings(directory.filePath("window.ini"), QSettings::IniFormat);
        // Use a plain window that fits the offscreen monitor. The production
        // layout above can legitimately exceed its 800-pixel width; Qt then
        // clamps it on restore, which is a different screen-recovery case.
        QMainWindow geometrySource;
        geometrySource.resize(600, 420); geometrySource.move(0, 0);
        geometrySource.show(); events();
        const auto normalSize = geometrySource.size();
        settings.setValue("window_geometry", geometrySource.saveGeometry().toBase64());
        settings.setValue("splitter_state", ui.splitter->saveState().toBase64());
        settings.sync();
        QMainWindow restored;
        MainWindowState::restoreWindow(&restored,
            QByteArray::fromBase64(settings.value("window_geometry").toByteArray()),
            0, 0, 0, 0, true, true);
        check(restored.size() == normalSize, "Window size INI round trip");
        QMainWindow ignored;
        ignored.resize(430, 330);
        MainWindowState::restoreWindow(&ignored, geometrySource.saveGeometry(), 600, 420, 0, 0, false, false);
        check(ignored.size() == QSize(430, 330), "Disabled geometry settings ignored");
        QMainWindow legacy;
        MainWindowState::restoreWindow(&legacy, {}, 600, 420, 0, 0, true, true);
        check(legacy.pos() == QPoint(0, 0), "Zero window position not restored");
        MainWindowState::restoreWindow(&legacy, "invalid geometry", 620, 440, 0, 0, true, true);
        check(legacy.size() == QSize(620, 440), "Corrupt geometry has no legacy fallback");
        MainWindowState::restoreWindow(&legacy, {}, 600, 420, -50000, -50000, true, true);
        check(legacy.pos().x() >= 0 && legacy.pos().y() >= 0, "Disconnected monitor leaves window unreachable");
        const QList<QRect> monitors{{0, 0, 1920, 1080}, {-1920, 0, 1920, 1080}, {0, -1080, 1920, 1080}};
        check(MainWindowState::reachableTitle(QRect(-1200, 0, 200, 32), monitors), "Left monitor position rejected");
        check(MainWindowState::reachableTitle(QRect(0, -800, 200, 32), monitors), "Upper monitor position rejected");
        check(!MainWindowState::reachableTitle(QRect(1919, 1079, 200, 32), monitors), "One visible pixel is not a reachable title bar");
        std::cout << "PASS: geometry INI round trip, zero coordinates, disabled restore and offscreen recovery\n";

        QTextDocument document;
        document.setUndoRedoEnabled(false);
        document.setMaximumBlockCount(2000);
        for (int i = 0; i < 6000; ++i)
            MainWindowState::appendLog(&document, QString("line-%1").arg(i));
        check(document.blockCount() == 2000, "Log document is unbounded");
        const auto text = document.toPlainText();
        check(text.startsWith("line-4000\n") && text.endsWith("line-5999"), "Log drops wrong history or leaves blank blocks");
        document.clear();
        MainWindowState::appendLog(&document, "fresh entry");
        check(document.toPlainText() == "fresh entry", "Empty first log line");
        QStringList pending;
        for (int i = 0; i < 40; ++i) pending.append(QString(1500, QChar('a' + i % 20)));
        const auto expected = pending.join('\n');
        QStringList batches;
        while (!pending.isEmpty()) {
            const auto batch = MainWindowState::takeLogBatch(pending);
            check(!batch.isEmpty() && batch.size() <= 16000, "Log batch silently exceeds massive-entry threshold");
            batches.append(batch);
        }
        check(batches.join('\n') == expected, "Log batching loses messages");
        std::cout << "PASS: bounded log history, no empty first line and lossless busy-log batching\n";
        std::cout << "PASS: main window UI regression checks\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
