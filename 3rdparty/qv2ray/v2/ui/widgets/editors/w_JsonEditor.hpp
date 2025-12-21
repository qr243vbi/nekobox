#pragma once

#include "../../../../wrapper.hpp"
#include "../common/QJsonModel.hpp"
#include "ui_w_JsonEditor.h"
#include <QMessageBox>
#include <QDialog>

#define QvMessageBoxWarn(a, b, c) QMessageBox::warning(a, b, c)

class JsonEditor
    : public QDialog,
      private Ui::JsonEditor {
    Q_OBJECT

public:
    explicit JsonEditor(const QJsonObject& rootObject, QWidget* parent = nullptr);
    ~JsonEditor();
    QJsonObject OpenEditor();

private slots:
    void on_jsonEditor_textChanged();

    void on_formatJsonBtn_clicked();

    void on_removeCommentsBtn_clicked();

private:
    QJsonModel model;
    QJsonObject original;
    QJsonObject final;
};
