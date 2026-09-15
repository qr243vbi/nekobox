#pragma once

#include <nekobox/dataStore/RouteEntity.h>
#include <QString>
#include <QStringList>

namespace LogRoute {

bool isErrorLine(const QString &line);

QStringList extractDomains(const QString &text);

// Returns empty string on success, error message otherwise.
QString addDomainToRoute(const QString &domain, Configs::simpleAction action,
                         const QString &matchType);

QString simpleActionLabel(Configs::simpleAction action);

} // namespace LogRoute
