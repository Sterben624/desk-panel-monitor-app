#include "bindingdialog.h"
#include "ui_bindingdialog.h"

BindingDialog::BindingDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::BindingDialog)
{
    ui->setupUi(this);
}

BindingDialog::~BindingDialog()
{
    delete ui;
}

void BindingDialog::setBindings(const QMap<QString, QString> &bindings)
{
    ui->leftEdit->setText(bindings.value("left"));
    ui->centerEdit->setText(bindings.value("center"));
    ui->rightEdit->setText(bindings.value("right"));
}

QMap<QString, QString> BindingDialog::bindings() const
{
    QMap<QString, QString> b;
    b["left"]   = ui->leftEdit->text();
    b["center"] = ui->centerEdit->text();
    b["right"]  = ui->rightEdit->text();
    return b;
}
