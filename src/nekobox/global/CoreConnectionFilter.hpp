#pragma once

#include <QDir>
#include <QFileInfo>
#include <QQueue>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace CoreConnectionFilter {
inline QString executableToken(QString path) {
    // RPC connection metadata contains a basename; process logs contain a path.
    path.replace('\\', '/');
    auto name = QFileInfo(path).completeBaseName().toLower();
    name.replace('-', '_');
    return name;
}

inline bool isInternalProcess(const QString &process) {
    const auto token = executableToken(process);
    return token == "xray" || token == "nekobox_core" || token == "nekoboxcore";
}

// Presentation only: never change routing, counters or the core's raw log.
class LogFilter {
public:
    QStringList filter(const QStringList &lines) {
        static const QRegularExpression processPath(
            QStringLiteral("found process path: (.*?)(?:, user(?: id)?: .*|$)"));
        static const QRegularExpression routineFlow(
            QStringLiteral("^(?:INFO|DEBUG|TRACE|NOTICE)\\[[^\\]]*\\]\\s+\\[(\\d+)(?:\\s|\\])"));
        static const QRegularExpression problem(
            QStringLiteral("\\b(?:warn(?:ing)?|error|fatal|panic|failed|failure)\\b"),
            QRegularExpression::CaseInsensitiveOption);
        // Discover IDs across the whole batch before filtering its earlier lines.
        for (const auto &line : lines) {
            const auto path = processPath.match(line);
            const auto flow = routineFlow.match(line);
            if (path.hasMatch() && flow.hasMatch() &&
                isInternalProcess(path.captured(1)) && !ids.contains(flow.captured(1))) {
                ids.insert(flow.captured(1));
                order.enqueue(flow.captured(1));
                if (order.size() > 4096) ids.remove(order.dequeue());
            }
        }
        QStringList visible;
        for (const auto &line : lines) {
            if (problem.match(line).hasMatch()) {
                visible.append(line);
                continue;
            }
            const auto flow = routineFlow.match(line);
            if (flow.hasMatch() && ids.contains(flow.captured(1))) continue;
            // Extra-core startup failures, warnings and errors remain visible.
            if (line.contains("Extra Core:") &&
                (line.contains("[Info]") || line.contains("[Debug]"))) continue;
            const auto started = line.indexOf(" : process started, waiting for it to end");
            if (started >= 0 && isInternalProcess(line.left(started).trimmed())) continue;
            visible.append(line);
        }
        return visible;
    }

private:
    QSet<QString> ids;
    QQueue<QString> order;
};
}
