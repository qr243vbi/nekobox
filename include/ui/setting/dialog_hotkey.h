#pragma once

#include <QDialog>
#include "include/global/Configs.hpp"
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

    void accept();

    void reject();

private:
    void generateShortcutItems(const QList<QAction*>& actions);
    QMap<QtExtKeySequenceEdit*, QString> seqEdit2ID;
    Ui::DialogHotkey *ui;
};
