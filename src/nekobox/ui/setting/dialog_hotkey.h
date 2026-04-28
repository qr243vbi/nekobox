#ifdef _WIN32
#include <winsock2.h>
#endif

#include <boost/bimap.hpp>
#include <qkeysequence.h>
#pragma once

#include <QDialog>
#include <nekobox/dataStore/Configs.hpp>
#include "ui_dialog_hotkey.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogHotkey;
}
QT_END_NAMESPACE

class DialogHotkey : public QDialog {
    Q_OBJECT

public:
    explicit DialogHotkey(QWidget *parent = nullptr, const QList<QAction*>& actions = {});

    ~DialogHotkey() override;

    public slots:

    void accept() override;

    void reject() override;

private:
    void generateShortcutItems(const QList<QAction*>& actions);
    boost::bimap<size_t, size_t> seqEdit2ID;
    QMap<QKeySequence, QAction*> labels;
    Ui::DialogHotkey *ui;
};
