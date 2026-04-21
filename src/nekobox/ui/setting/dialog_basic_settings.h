#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#ifndef DIALOG_BASIC_SETTINGS_H
#define DIALOG_BASIC_SETTINGS_H

#include <QDialog>
#include <QJsonObject>
#include "ui_dialog_basic_settings.h"
#include <nekobox/ui/mainwindow.h>



#include <QDialog>
#include <QListView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDebug>

#include <QAbstractListModel>
#include <QList>
#include <nekobox/sys/Settings.h>

class LanguageModel : public QAbstractListModel
{
    Q_OBJECT

public:
    LanguageModel(QObject *parent = nullptr)
        : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;


    std::shared_ptr<LanguageValue> getLanguage(int index) const ;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    QList<std::shared_ptr<LanguageValue>> languageList = languageCodes(); // The list of shared pointers
};

class LanguageSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    LanguageSelectionDialog(QWidget *parent = nullptr);

    std::shared_ptr<LanguageValue> getSelectedLanguage() const ;

private slots:
    void onOkClicked();

private:
    QListView *languageListView;
    LanguageModel *languageModel;
    std::shared_ptr<LanguageValue> selectedLanguage;
};



namespace Ui {
    class DialogBasicSettings;
}

class DialogBasicSettings : public QDialog {
    Q_OBJECT

public:
    explicit DialogBasicSettings(MainWindow *parent = nullptr);

    ~DialogBasicSettings();

    MainWindow * parent;

signals:
    void size_changed(int x, int y);
    void point_changed(int x, int y);

public slots:

    void accept();

private:
    Ui::DialogBasicSettings *ui;

    struct {
        QString language_code;
        QString custom_inbound;
        QString core_path_old;
        bool needRestart = false;
        bool updateDisableTray = false;
        bool updateSystemDns = false;
        bool updateIcon = false;
        bool updateFont = false;
        bool updateMenuIcon = false;
    } CACHE;

private slots:
    void on_core_settings_clicked();
};

#endif // DIALOG_BASIC_SETTINGS_H
