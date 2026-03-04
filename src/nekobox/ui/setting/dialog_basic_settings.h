#ifndef DIALOG_BASIC_SETTINGS_H
#define DIALOG_BASIC_SETTINGS_H

#include <QDialog>
#include <QJsonObject>
#include "ui_dialog_basic_settings.h"
#include "nekobox/ui/mainwindow.h"



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

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return languageList.size(); // The number of items in the list
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (index.isValid() && index.row() < languageList.size()) {
            const auto &language = languageList[index.row()];
            if (role == Qt::DisplayRole) {
                return language->name;
            } else if (role == 9999){
                return language->code;
            }
        }
        return QVariant();
    }

private:
    QList<std::shared_ptr<LanguageValue>> languageList = languageCodes(); // The list of shared pointers
};

class LanguageSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    LanguageSelectionDialog(const QList<std::shared_ptr<LanguageValue>> &languages, QWidget *parent = nullptr)
        : QDialog(parent), selectedLanguage(nullptr)
    {
        // Set dialog title
        setWindowTitle("Select Language");

        // Create layout
        QVBoxLayout *layout = new QVBoxLayout(this);

        // Create the list view to show available languages
        languageListView = new QListView(this);
        layout->addWidget(languageListView);

        // Create the model to display the languages
        languageModel = new LanguageModel(this);

        // Set the model for the list view
        languageListView->setModel(languageModel);

        // Enable selection
        languageListView->setSelectionMode(QAbstractItemView::SingleSelection);

        // Create OK and Cancel buttons
        QPushButton *okButton = new QPushButton("OK", this);
        QPushButton *cancelButton = new QPushButton("Cancel", this);

        // Connect buttons to actions
        connect(okButton, &QPushButton::clicked, this, &LanguageSelectionDialog::onOkClicked);
        connect(cancelButton, &QPushButton::clicked, this, &LanguageSelectionDialog::reject);

        // Add buttons to layout
        layout->addWidget(okButton);
        layout->addWidget(cancelButton);

        // Set the layout for the dialog
        setLayout(layout);
    }

    std::shared_ptr<LanguageValue> getSelectedLanguage() const {
        return selectedLanguage;
    }

private slots:
    void onOkClicked()
    {
        QModelIndex selectedIndex = languageListView->currentIndex();
        if (selectedIndex.isValid()) {
            selectedLanguage = languageModel->data(selectedIndex, Qt::DisplayRole).value<std::shared_ptr<LanguageValue>>();
            qDebug() << "Selected language:" << selectedLanguage->name;
            accept(); // Close the dialog with acceptance
        } else {
            qDebug() << "No language selected";
        }
    }

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
        QString custom_inbound;
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
