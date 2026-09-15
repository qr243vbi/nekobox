#include <nekobox/global/LogRouteHelper.hpp>

#include <nekobox/dataStore/Database.hpp>
#include <QObject>
#include <QRegularExpression>
#include <QSet>

namespace LogRoute {

static bool looksLikeIpv4(const QString &value) {
  static const QRegularExpression re(
      R"(^(?:\d{1,3}\.){3}\d{1,3}$)");
  const auto match = re.match(value);
  if (!match.hasMatch()) {
    return false;
  }
  for (const QString &part : value.split('.')) {
    bool ok = false;
    const int octet = part.toInt(&ok);
    if (!ok || octet < 0 || octet > 255) {
      return false;
    }
  }
  return true;
}

static bool looksLikeDomain(const QString &candidate) {
  const QString dom = candidate.trimmed().toLower();
  if (dom.size() < 4 || dom.size() > 253) {
    return false;
  }
  if (!dom.contains('.')) {
    return false;
  }
  if (dom.startsWith('[')) {
    return false;
  }
  if (looksLikeIpv4(dom)) {
    return false;
  }
  static const QRegularExpression re(
      R"(^[a-z0-9](?:[a-z0-9\-]*[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9\-]*[a-z0-9])?)+$)");
  return re.match(dom).hasMatch();
}

static void collectDomain(QSet<QString> *seen, QStringList *out,
                          const QString &candidate) {
  const QString dom = candidate.trimmed().toLower();
  if (!looksLikeDomain(dom) || seen->contains(dom)) {
    return;
  }
  seen->insert(dom);
  out->append(dom);
}

bool isErrorLine(const QString &line) {
  static const QRegularExpression re(
      R"((?i)(deadline exceeded|context deadline|i/o timeout|connection refused|connection reset|no such host|lookup .+ failed|name or service not known|tls handshake timeout|network is unreachable|operation timed out|dial tcp|dial udp|read tcp|write tcp|\.connect\(\)| handshake failed|remote error|protocol error|exchange failed|unexpected eof|\berror\b|\bfailed\b))");
  return re.match(line).hasMatch();
}

QStringList extractDomains(const QString &text) {
  QStringList domains;
  QSet<QString> seen;

  static const QRegularExpression domainTag(
      R"((?i)(?:domain|host|sni|server name)[=:\s]+([a-z0-9](?:[a-z0-9\-]*[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9\-]*[a-z0-9])?)+))");
  static const QRegularExpression urlHost(
      R"(https?://([a-z0-9](?:[a-z0-9\-]*[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9\-]*[a-z0-9])?)+))");
  static const QRegularExpression hostPort(
      R"((?i)(?:to |for |via |@)([a-z0-9](?:[a-z0-9\-]*[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9\-]*[a-z0-9])?)+)(?::\d+)?)");
  static const QRegularExpression dialHost(
      R"((?i)dial (?:tcp|udp)(?:4|6)? ([a-z0-9](?:[a-z0-9\-]*[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9\-]*[a-z0-9])?)+)(?::\d+)?)");

  auto it = domainTag.globalMatch(text);
  while (it.hasNext()) {
    collectDomain(&seen, &domains, it.next().captured(1));
  }
  it = urlHost.globalMatch(text);
  while (it.hasNext()) {
    collectDomain(&seen, &domains, it.next().captured(1));
  }
  it = hostPort.globalMatch(text);
  while (it.hasNext()) {
    collectDomain(&seen, &domains, it.next().captured(1));
  }
  it = dialHost.globalMatch(text);
  while (it.hasNext()) {
    collectDomain(&seen, &domains, it.next().captured(1));
  }

  return domains;
}

QString addDomainToRoute(const QString &domain, Configs::simpleAction action,
                         const QString &matchType) {
  auto chain = Configs::profileManager->GetRouteChain(
      Configs::dataStore->routing->current_route_id);
  if (chain == nullptr) {
    return QObject::tr("No route profile selected. Open Route Settings first.");
  }

  QString prefix;
  if (matchType == "domain") {
    prefix = "domain:";
  } else if (matchType == "keyword") {
    prefix = "keyword:";
  } else {
    prefix = "suffix:";
  }

  const QString entry = prefix + domain.trimmed().toLower();
  QString content = chain->GetSimpleRules(action);
  for (const auto &line : content.split('\n', Qt::SkipEmptyParts)) {
    if (line.trimmed() == entry) {
      return {};
    }
  }

  if (!content.isEmpty()) {
    content += "\n";
  }
  content += entry;

  const QString err = chain->UpdateSimpleRules(content, action);
  if (!err.isEmpty()) {
    return err;
  }

  chain->Save();
  return {};
}

QString simpleActionLabel(Configs::simpleAction action) {
  if (action == Configs::direct) {
    return QObject::tr("Direct");
  }
  if (action == Configs::proxy) {
    return QObject::tr("Proxy");
  }
  if (action == Configs::block) {
    return QObject::tr("Block");
  }
  return {};
}

} // namespace LogRoute
