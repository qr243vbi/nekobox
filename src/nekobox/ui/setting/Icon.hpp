



#pragma once
#include <QToolButton>
#include <QPixmap>
#include <QTimer>
#include <QToolButton>
#include <QPropertyAnimation>
#include <QColor>
#include <QPainter>


#pragma once

#include <QToolButton>
#include <QPropertyAnimation>
#include <QColor>


namespace Icon {

    enum TrayIconStatus {
        NONE = 0,
        PROXY = 1,
        SYSTEM_PROXY = 2,
        VPN = 3,
        DNS = 4,
        SYSTEM_PROXY_DNS = 5,
    };

    enum class State {
        IDLE,
        CONNECTING,
        RUNNING,
        DISCONNECTING
    };

    QPixmap GetTrayIcon(TrayIconStatus status = TrayIconStatus::NONE) ;

    class StatusControlButton : public QToolButton {
        Q_OBJECT
        Q_PROPERTY(qreal morph READ morph WRITE setMorph)
    Q_PROPERTY(qreal spin READ spin WRITE setSpin)
    Q_PROPERTY(qreal glow READ glow WRITE setGlow)
    Q_PROPERTY(qreal dim READ dim WRITE setDim)
    Q_PROPERTY(qreal press READ press WRITE setPress)
    Q_PROPERTY(QColor ringColor READ ringColor WRITE setRingColor)

public:

    Q_ENUM(State)

    Q_ENUM(TrayIconStatus)

    explicit StatusControlButton(QWidget *parent = nullptr);

    void setState(State s);
    State state() const { return m_state; }

    void setMode(Icon::TrayIconStatus m);
    Icon::TrayIconStatus mode() const { return m_mode; }

    QSize minimumSizeHint() const override { return sizeHint(); }

    // --- animated properties ---
    qreal morph() const { return m_morph; }
    void setMorph(qreal v) { m_morph = v; update(); }
    qreal spin() const { return m_spin; }
    void setSpin(qreal v) { m_spin = v; update(); }
    qreal glow() const { return m_glow; }
    void setGlow(qreal v) { m_glow = v; update(); }
    qreal dim() const { return m_dim; }
    void setDim(qreal v) { m_dim = v; update(); }
    qreal press() const { return m_press; }
    void setPress(qreal v) { m_press = v; update(); }
    QColor ringColor() const { return m_ringColor; }
    void setRingColor(const QColor &c) { m_ringColor = c; update(); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void applyState(bool animated);
    void animate(QPropertyAnimation *anim, const QVariant &to, int duration);
    void setLoopRunning(QPropertyAnimation *anim, bool running);

    QColor modeColor(Icon::TrayIconStatus m) const;
    QColor idleRingColor() const;
    QColor glyphColor() const;
    QColor targetRingColor() const;

    State m_state = State::IDLE;
    Icon::TrayIconStatus m_mode = Icon::TrayIconStatus::NONE;

    qreal m_morph = 0.0;
    qreal m_spin = 0.0;
    qreal m_glow = 0.0;
    qreal m_dim = 1.0;
    qreal m_press = 0.0;
    QColor m_ringColor;

    QPropertyAnimation *m_morphAnim = nullptr;
    QPropertyAnimation *m_dimAnim = nullptr;
    QPropertyAnimation *m_pressAnim = nullptr;
    QPropertyAnimation *m_ringColorAnim = nullptr;
    QPropertyAnimation *m_spinAnim = nullptr;
    QPropertyAnimation *m_glowAnim = nullptr;
};

}

using Icon::StatusControlButton;