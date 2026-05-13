
#include <nekobox/ui/setting/dialog_hotkey.h>
#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/global/GuiUtils.hpp>

#include <nekobox/ui/mainwindow_interface.h>
#include <nekobox/global/keyvaluerange.h>
#include <nekobox/sys/Settings.h>
#include <QAction>

DialogHotkey::DialogHotkey(QWidget *parent, const QList<QAction*>& actions) : QDialog(parent), ui(new Ui::DialogHotkey) {
    CHECK_SETTINGS_ACCESS
    ui->setupUi(this);
    ui->show_mainwindow->setKeySequence(Configs::dataStore->hotkey_mainwindow);
    ui->show_groups->setKeySequence(Configs::dataStore->hotkey_group);
    ui->show_routes->setKeySequence(Configs::dataStore->hotkey_route);
    ui->system_proxy->setKeySequence(Configs::dataStore->hotkey_system_proxy_menu);
    ui->toggle_proxy->setKeySequence(Configs::dataStore->hotkey_toggle_system_proxy);

#ifndef USE_HOTKEYS
    ui->global->hide();
#endif

    generateShortcutItems(actions);

    GetMainWindow()->RegisterHotkey(true);
}

void DialogHotkey::generateShortcutItems(const QList<QAction*>& actions)
{
    auto widget = new QWidget(ui->shortcut_area);
    labels.clear();
    seqEdit2ID.clear();
    auto layout = new QFormLayout(widget);
    widget->setLayout(layout);
    ui->shortcut_area->setWidget(widget);
    for (auto action : actions)
    {
        auto kseq = new QtExtKeySequenceEdit(this);
        QKeySequence shortcut = action->shortcut();
        if (!shortcut.isEmpty()) kseq->setKeySequence(shortcut);
        seqEdit2ID.insert({(size_t)(void*)kseq, (size_t)(void*)action});
        QString text = action->text();
        layout->addRow(text, kseq);
        if (labels.count(shortcut) == 0){
            labels[shortcut] = action;
        }
        connect(kseq, &QtExtKeySequenceEdit::keySequenceChanged, this, 
            [this, action](const QKeySequence &key)->void{
            if (key != 0 && labels.count(key) > 0){
                auto second_action = labels[key];
                if (second_action == action){
                    return;
                }
                second_action->setShortcut(0);
                ((QtExtKeySequenceEdit*)(void*)seqEdit2ID.right.at((size_t)(void*)second_action))->setKeySequence(0);
                MessageBoxWarning(software_name, tr(
                    "Shortcut for '%s' action was cleared").replace("%s", second_action->text()));
            };
            labels[key] = action;
        });
    }
}

void DialogHotkey::accept()
{
    Configs::dataStore->hotkey_mainwindow = ui->show_mainwindow->keySequence().toString();
    Configs::dataStore->hotkey_group = ui->show_groups->keySequence().toString();
    Configs::dataStore->hotkey_route = ui->show_routes->keySequence().toString();
    Configs::dataStore->hotkey_system_proxy_menu = ui->system_proxy->keySequence().toString();
    Configs::dataStore->hotkey_toggle_system_proxy = ui->toggle_proxy->keySequence().toString();

    for (auto [kseq, actionID] : seqEdit2ID)
    {
        Configs::windowSettings->shortcuts->shortcuts[((QAction*)(void*)actionID)->data().toString()] = 
            ((QtExtKeySequenceEdit*)(void*)kseq)->keySequence();
    }
    Configs::windowSettings->shortcuts->Save();

    Configs::dataStore->Save();
    MW_dialog_message(Dialog_DialogManageHotkeys, "UpdateShortcuts");
    GetMainWindow()->RegisterHotkey(false);
    QDialog::accept();
}

void DialogHotkey::reject()
{
    GetMainWindow()->RegisterHotkey(false);
    QDialog::reject();
}

DialogHotkey::~DialogHotkey() {
    delete ui;
}
