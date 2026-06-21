#include <nekobox/ui/setting/Icon.hpp>
#include <nekobox/global/GuiUtils.hpp>
#include <nekobox/ui/info/info.h>
#ifdef NKR_SOFTWARE_KEYS
#include <nekobox/ui/mainwindow.h>
#include <nekobox/ui/security_addon.h>
#else
#define CHECK_SETTINGS_ACCESS
#endif
#include <QCoreApplication>
#include <QDir>
#include <QPainter>
#include <nekobox/stats/traffic/TrafficLooper.hpp>
#include <nekobox/dataStore/Database.hpp>
#include <qicon.h>
#include <QStylePainter>
#include <QPainterPath>

#ifndef SYSTRAY_ICON_DIR
#include <nekobox/sys/Settings.h>
#define SYSTRAY_ICON(X) getResource(X)
#endif

using Configs::indicatorRuleMap;
using Configs::QListInt2Color;

QPixmap Icon::GetTrayIcon(TrayIconStatus status) {
  QPixmap pixmap(256, 256);
  pixmap.fill(Qt::transparent);

  QIcon pixmap_read(SYSTRAY_ICON("icon.png"));
  bool pixmap_read_isnull = pixmap_read.isNull();
  auto p = QPainter(&pixmap);
  auto rule = indicatorRuleMap[status];
  if (pixmap_read_isnull) {
    pixmap_read = QIcon::fromTheme("nekobox");
    pixmap_read_isnull = pixmap_read.isNull();
  } 
  if (!pixmap_read_isnull){
    p.drawPixmap(0, 0, pixmap_read.pixmap(QSize(256, 256)));
    if (indicatorRuleMap.contains(status)) {
      int side = pixmap.width();
      int radius = side * rule.radius;
      int d = side * rule.diameter;
      int margin = side * rule.margin;

#ifdef DEBUG_MODE
      qDebug() << "ICON side" << side << "radius" << radius << "diameter" << d << "margin" << margin;
#endif
      if (radius > 0) {
        p.setBrush(QBrush(QListInt2Color(rule.color)));
        p.drawRoundedRect(QRect(side - d - margin,
           side - d - margin, d, d), radius, radius);
      }
    }
  }
  p.end();
  return pixmap;
}

#define SET_CUSTOM_STAT(name, value, FUNC) this->ui->name->setText(FUNC(value));
#define SET_LOGGER_STAT(name, FUNC) SET_CUSTOM_STAT(name, Stats::databaseLogger->name, FUNC);
#define SET_LOGGER_FUNC(name, FUNC) SET_CUSTOM_STAT(name, Stats::databaseLogger->get_##name(), FUNC);
#define SET_TRAFFIC_STAT(type, way, FUNC) SET_CUSTOM_STAT(total_##type##_##way##load, \
  Stats::trafficLooper->total_##type##_##way##load() + Stats::databaseLogger->total_##type->way##link, FUNC);
#define SET_DATA_STAT(proxy, profiles, WORD, FUNC) \
    SET_CUSTOM_STAT(proxy##_created, Stats::databaseLogger->profiles->created, FUNC); \
    SET_CUSTOM_STAT(proxy##_deleted, Stats::databaseLogger->profiles->deleted, FUNC); \
    SET_CUSTOM_STAT(proxy##_exists, WORD, FUNC); 

InfoDialog::InfoDialog(QWidget *parent) : QDialog(parent), ui(new Ui::InfoMain)  {
  CHECK_SETTINGS_ACCESS
  ui->setupUi(this);
 // ui->textBrowser->document()->setDefaultFont(qApp->font());
 // ui->textBrowser->setOpenExternalLinks(true);
  this->setWindowTitle(software_name);
  SET_TRAFFIC_STAT(direct, down, ReadableSize)
  SET_TRAFFIC_STAT(direct, up, ReadableSize)
  SET_TRAFFIC_STAT(proxy, down, ReadableSize)
  SET_TRAFFIC_STAT(proxy, up, ReadableSize)
  SET_LOGGER_STAT(start_count, QString::number)

  SET_DATA_STAT(proxy, profiles, Configs::profileManager->getGroupCount(), QString::number)
  SET_DATA_STAT(group, groups, Configs::profileManager->groups.size(), QString::number)
  SET_DATA_STAT(route, routes, Configs::profileManager->routes.size(), QString::number)
  SET_LOGGER_FUNC(usage_time, ReadableDuration)
  SET_LOGGER_STAT(last_launch_time, ReadableDateTime)
  SET_LOGGER_STAT(first_launch_time, ReadableDateTime)
  
#ifdef NKR_SOFTWARE_KEYS
  SET_CUSTOM_STAT(users_count, getUserCount(), QString::number)
  SET_LOGGER_FUNC(failed_auth_count, QString::number)
#else 
  this->ui->security_group->hide();
  #endif
  
//  this->ui->total_direct_download->setText(Stats::trafficLooper->);
//  this->ui->total_direct_upload->setText();
//  this->ui->total_proxy_download->setText();
//  this->ui->total_proxy_upload->setText();
}

InfoDialog::~InfoDialog(){

}

void InfoDialog::accept(){

}


AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent), ui(new Ui::AboutMain)  {
  ui->setupUi(this);
  ui->textBrowser->setText(ReadFileText(getResource("about.html")));
  ui->textBrowser->document()->setDefaultFont(qApp->font());
  ui->textBrowser->setOpenExternalLinks(true);
  this->setWindowTitle(software_name);
}

AboutDialog::~AboutDialog(){

}

void AboutDialog::accept(){

}


























#include <QConicalGradient>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QStyleOptionToolButton>
#include <QtMath>
#include <QStylePainter>

#define StartStopButton StatusControlButton

StartStopButton::StartStopButton(QWidget *parent) : QToolButton(parent) {
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_ringColor = idleRingColor();

    m_morphAnim = new QPropertyAnimation(this, "morph", this);
    m_dimAnim = new QPropertyAnimation(this, "dim", this);
    m_pressAnim = new QPropertyAnimation(this, "press", this);
    m_ringColorAnim = new QPropertyAnimation(this, "ringColor", this);

    // Looping animations: the connecting spinner and the running glow.
    m_spinAnim = new QPropertyAnimation(this, "spin", this);
    m_spinAnim->setStartValue(0.0);
    m_spinAnim->setEndValue(360.0);
    m_spinAnim->setDuration(900);
    m_spinAnim->setLoopCount(-1);
    m_spinAnim->setEasingCurve(QEasingCurve::Linear);

    // glow is a 0..1 phase advanced at a constant rate; paint maps it through a
    // raised cosine, so the breath rises and falls at equal rates and dwells
    // equally at bright and dim (InOutSine here lingered dim and rushed the peak).
    m_glowAnim = new QPropertyAnimation(this, "glow", this);
    m_glowAnim->setStartValue(0.0);
    m_glowAnim->setEndValue(1.0);
    m_glowAnim->setDuration(3400);
    m_glowAnim->setLoopCount(-1);
    m_glowAnim->setEasingCurve(QEasingCurve::Linear);

    connect(this, &QAbstractButton::pressed, this, [this] { animate(m_pressAnim, 1.0, 110); });
    connect(this, &QAbstractButton::released, this, [this] { animate(m_pressAnim, 0.0, 160); });

    // Establish the initial visuals without an entry animation.
    m_state = State::IDLE;
    applyState(false);
}

void StartStopButton::setState(State s) {
    if (s == m_state) return;
    m_state = s;
    applyState(true);
}

void StartStopButton::setMode(Icon::TrayIconStatus m) {
    if (m == m_mode) return;
    m_mode = m;
    // The mode colour only shows while running; animate the ring if it's live.
    if (m_state == State::RUNNING) animate(m_ringColorAnim, targetRingColor(), 320);
}

void StartStopButton::applyState(bool animated) {
    // IDLE and RUNNING are clickable; Connecting (no cancel) and Disabled are not.
    const bool interactive = (m_state == State::IDLE || m_state == State::RUNNING);
    setEnabled(interactive);
    setCursor(interactive ? Qt::PointingHandCursor : Qt::ArrowCursor);

    switch (m_state) {
        case State::IDLE: setToolTip(tr("Start")); break;
        case State::CONNECTING: setToolTip(tr("Connecting…")); break;
        case State::RUNNING: setToolTip(tr("Stop")); break;
        case State::DISCONNECTING: setToolTip(tr("Stopping…")); break;
    }

    const qreal morphTarget = (m_state == State::RUNNING || m_state == State::DISCONNECTING) ? 1.0 : 0.0;
    const qreal dimTarget = (m_state == State::IDLE) ? 0.87 : 1.0;
    const QColor ringTarget = targetRingColor();

    if (animated) {
        animate(m_morphAnim, morphTarget, 300);
        animate(m_dimAnim, dimTarget, 320);
        animate(m_ringColorAnim, ringTarget, 320);
    } else {
        m_morphAnim->stop();
        m_dimAnim->stop();
        m_ringColorAnim->stop();
        m_morph = morphTarget;
        m_dim = dimTarget;
        m_ringColor = ringTarget;
    }

    setLoopRunning(m_spinAnim, m_state == State::CONNECTING || m_state == State::DISCONNECTING);
    setLoopRunning(m_glowAnim, m_state == State::RUNNING);
    update();
}

void StartStopButton::animate(QPropertyAnimation *anim, const QVariant &to, int duration) {
    anim->stop();
    anim->setDuration(duration);
    anim->setEasingCurve(QEasingCurve::InOutCubic);
    anim->setStartValue(property(anim->propertyName().constData()));
    anim->setEndValue(to);
    anim->start();
}

void StartStopButton::setLoopRunning(QPropertyAnimation *anim, bool running) {
    if (running) {
        if (anim->state() != QAbstractAnimation::Running) anim->start();
        return;
    }
    anim->stop();
    if (anim == m_spinAnim) m_spin = 0.0;
    if (anim == m_glowAnim) m_glow = 0.0;
    update();
}

QColor StartStopButton::modeColor(Icon::TrayIconStatus m) const {
    auto iter = indicatorRuleMap.find(m);
    if (iter == indicatorRuleMap.end()) {
        return idleRingColor();
    } else {
        return QColor(QListInt2Color(iter->second.color));
    }
}

QColor StartStopButton::idleRingColor() const {
    QColor c = palette().color(QPalette::WindowText);
    c.setAlphaF(0.12);
    return c;
}

QColor StartStopButton::glyphColor() const {
    if (m_state == State::RUNNING || m_state == State::DISCONNECTING) return {0x99, 0x46, 0x46};
    return Qt::darkGreen;
}

QColor StartStopButton::targetRingColor() const {
    switch (m_state) {
        case State::CONNECTING:
        case State::DISCONNECTING: return {0xFF, 0xB3, 0x2C};
        case State::RUNNING: return modeColor(m_mode);
        default: return idleRingColor();
    }
}


void StartStopButton::paintEvent(QPaintEvent *) {
    QStylePainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QStyleOptionToolButton opt;
    initStyleOption(&opt);
    opt.text.clear();
    opt.icon = QIcon();
    opt.iconSize = QSize();
    opt.features &= ~QStyleOptionToolButton::HasMenu;
    opt.subControls &= ~QStyle::SC_ToolButtonMenu;
    opt.arrowType = Qt::NoArrow;
    if (m_state == State::CONNECTING || m_state == State::DISCONNECTING) {
        opt.state |= QStyle::State_Enabled;
        opt.state &= ~QStyle::State_Sunken;
    }
    p.drawComplexControl(QStyle::CC_ToolButton, opt);

    const QRectF cr = contentsRect();
    const qreal D = qMin(cr.width(), cr.height());
    const QPointF c = cr.center();

    const qreal scale = 1.0 - 0.06 * m_press;
    p.translate(c);
    p.scale(scale, scale);
    p.translate(-c);

    const qreal penW = qMax(1.6, D * 0.063);
    const qreal R = D * 0.34;
    const QRectF rr(c.x() - R, c.y() - R, 2 * R, 2 * R);

    p.setOpacity(m_dim);

    if (m_state == State::CONNECTING || m_state == State::DISCONNECTING) {
        p.setBrush(Qt::NoBrush);
        QColor track = m_ringColor;
        track.setAlphaF(0.75);
        QPen trackPen(track, penW);
        p.setPen(trackPen);
        p.drawEllipse(c, R, R);

        QPen arcPen(m_ringColor, penW);
        arcPen.setCapStyle(Qt::RoundCap);
        p.setPen(arcPen);
        const int startAngle = static_cast<int>(-m_spin * 16);
        const int spanAngle = -110 * 16;
        p.drawArc(rr, startAngle, spanAngle);
    } else if (m_state == State::RUNNING) {
        const qreal pulse = 0.5 - 0.5 * qCos(m_glow * 2.0 * M_PI);

        const qreal glowR = R + penW * 2.4;
        const qreal ringStop = R / glowR;
        QColor gPeak = m_ringColor;
        gPeak.setAlphaF(0.20 + 0.40 * pulse);
        QColor gEdge = m_ringColor;
        gEdge.setAlphaF(0.0);
        QRadialGradient g(c, glowR);
        g.setColorAt(0.0, gEdge);
        g.setColorAt(ringStop * 0.9, gEdge);
        g.setColorAt(ringStop, gPeak);
        g.setColorAt(1.0, gEdge);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(c, glowR, glowR);

        p.setBrush(Qt::NoBrush);
        QColor base = m_ringColor.lighter(static_cast<int>(101 + 9 * pulse));
        base.setAlphaF(0.95);
        QConicalGradient cg(c, 90.0);
        cg.setColorAt(0.0, base.lighter(116));
        cg.setColorAt(0.5, base);
        cg.setColorAt(1.0, base.lighter(116));
        QPen ringPen(QBrush(cg), penW);
        ringPen.setCapStyle(Qt::RoundCap);
        p.setPen(ringPen);
        p.drawEllipse(c, R, R);
    } else {
        p.setBrush(Qt::NoBrush);
        QPen ringPen(m_ringColor, penW);
        ringPen.setCapStyle(Qt::RoundCap);
        p.setPen(ringPen);
        p.drawEllipse(c, R, R);
    }

    const qreal h = D * 0.136;
    const qreal t = m_morph;
    auto lerp = [](const QPointF &a, const QPointF &b, qreal k) { return a + (b - a) * k; };
    const qreal triShift = h / 3.0;
    const QPointF tri[4] = {
        c + QPointF(-h + triShift, -h),
        c + QPointF(h + triShift, 0),
        c + QPointF(h + triShift, 0),
        c + QPointF(-h + triShift, h),
    };
    const QPointF sq[4] = {
        c + QPointF(-h, -h),
        c + QPointF(h, -h),
        c + QPointF(h, h),
        c + QPointF(-h, h),
    };
    QPainterPath path;
    path.moveTo(lerp(tri[0], sq[0], t));
    for (int i = 1; i < 4; ++i) path.lineTo(lerp(tri[i], sq[i], t));
    path.closeSubpath();

    QLinearGradient lg(c.x(), c.y() - h, c.x(), c.y() + h);
    const QColor g1 = glyphColor();
    lg.setColorAt(0.0, g1.lighter(118));
    lg.setColorAt(1.0, g1.darker(112));
    const qreal corner = D * 0.04;
    QPen gpen(QBrush(lg), corner);
    gpen.setJoinStyle(Qt::RoundJoin);
    gpen.setCapStyle(Qt::RoundCap);
    const qreal glyphAlpha = (m_state == State::CONNECTING || m_state == State::DISCONNECTING) ? 0.5 : 1.0;
    p.setOpacity(m_dim * glyphAlpha);
    p.setPen(gpen);
    p.setBrush(QBrush(lg));
    p.drawPath(path);
}
