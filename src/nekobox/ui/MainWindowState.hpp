#pragma once

#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QSplitter>
#include <QStringList>
#include <QTextCursor>
#include <QTextDocument>
#include <QWidget>

namespace MainWindowState {

inline QString displayName(QString configured, QString fallback = {}) {
    QString name = configured.trimmed();
    if (name.isEmpty()) name = fallback.trimmed();
    const QStringList legacyNames{"", "nekobox", "nyamebox", "iblis", "nekoray", "qr243vbi"};
    return legacyNames.contains(name.toLower()) ? QStringLiteral("NekoBox") : name;
}

inline void restoreSplitter(QSplitter *splitter, const QByteArray &state) {
    // QTabWidget includes the minimum height of *hidden* pages in its hint.
    // The two route tables must not prevent shrinking the visible log page.
    for (int i = 0; i < splitter->count(); ++i) {
        auto *pane = splitter->widget(i);
        auto policy = pane->sizePolicy();
        policy.setVerticalPolicy(QSizePolicy::Ignored);
        pane->setSizePolicy(policy);
        pane->setMinimumHeight(i == 1 ? 96 : 64);
    }
    const bool restored = !state.isEmpty() && splitter->restoreState(state);
    splitter->setHandleWidth(8);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    const auto sizes = splitter->sizes();
    if (!restored || sizes.size() != 2 || sizes[0] < 64 || sizes[1] < 64)
        splitter->setSizes({360, 180});
    if (splitter->count() > 1) {
        splitter->handle(1)->setCursor(Qt::SplitVCursor);
    }
}

inline QScrollArea *scrollPanel(QWidget *content, QWidget *parent) {
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    scroll->setWidget(content);
    return scroll;
}

inline bool reachableTitle(const QRect &title, const QList<QRect> &screens) {
    for (const auto &screen : screens) {
        const auto visible = screen.intersected(title);
        if (visible.width() >= 64 && visible.height() >= 24) return true;
    }
    return false;
}

inline void restoreWindow(QWidget *window, const QByteArray &geometry,
                          int width, int height, int x, int y,
                          bool saveSize, bool savePosition) {
    if (saveSize && savePosition && !geometry.isEmpty() && window->restoreGeometry(geometry))
        return;
    if (saveSize && width > 0 && height > 0) window->resize(width, height);
    if (!savePosition || (width <= 0 && height <= 0 && x == 0 && y == 0)) return;
    const QRect titleArea(x, y, qMax(64, qMin(window->width(), 200)), 32);
    QList<QRect> screens;
    for (auto *screen : QGuiApplication::screens()) screens << screen->availableGeometry();
    if (reachableTitle(titleArea, screens)) {
        window->move(x, y); // zero and negative multi-monitor coordinates are valid
        return;
    }
    if (auto *screen = QGuiApplication::primaryScreen()) {
        const auto available = screen->availableGeometry();
        window->resize(window->size().boundedTo(available.size()));
        window->move(available.topLeft() + QPoint(24, 24));
    }
}

inline void appendLog(QTextDocument *document, const QString &message) {
    QTextCursor cursor(document);
    cursor.movePosition(QTextCursor::End);
    cursor.beginEditBlock();
    if (!document->isEmpty()) cursor.insertBlock();
    cursor.insertText(message);
    cursor.endEditBlock();
}

inline QString takeLogBatch(QStringList &pending, int limit = 16000) {
    QString batch;
    while (!pending.isEmpty()) {
        const auto &next = pending.first();
        if (!batch.isEmpty() && batch.size() + 1 + next.size() > limit) break;
        if (!batch.isEmpty()) batch += QLatin1Char('\n');
        batch += pending.takeFirst();
        if (batch.size() >= limit) break;
    }
    return batch;
}
}
