#pragma once

#include <QDialog>
#include <QMap>
#include <QString>

namespace Ui {
class BindingDialog;
}

class BindingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BindingDialog(QWidget *parent = nullptr);
    ~BindingDialog();

    void setBindings(const QMap<QString, QString> &bindings);
    QMap<QString, QString> bindings() const;

private:
    Ui::BindingDialog *ui;
};
