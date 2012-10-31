#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"

PreferencesDialog::PreferencesDialog(QWidget *parent) :
    QDialog(parent), ui(new Ui::PreferencesDialog)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    ui->setupUi(this);
    QtHelpers::GenAdjustWidgetAppearanceToOS(this);

    settings = new QSettings("Exetelek", "Velocity");

    ui->comboBox->setCurrentIndex(settings->value("PackageDropAction").toInt());
    ui->comboBox_2->setCurrentIndex(settings->value("ProfileDropAction").toInt());
    ui->lineEdit->setText(settings->value("PluginPath").toString());

    setFixedSize(sizeHint());
}

PreferencesDialog::~PreferencesDialog()
{
    delete settings;
    delete ui;
}

void PreferencesDialog::on_pushButton_clicked()
{
    settings->setValue("PackageDropAction", ui->comboBox->currentIndex());
    settings->setValue("ProfileDropAction", ui->comboBox_2->currentIndex());
    settings->setValue("PluginPath", ui->lineEdit->text());

    close();
}

void PreferencesDialog::on_pushButton_4_clicked()
{
    QString directory = QFileDialog::getExistingDirectory(this, "Choose the plugin path", QtHelpers::ExecutingDirectory());

    if (!directory.isNull())
        return;

    QDir dir(QtHelpers::ExecutingDirectory());
    ui->lineEdit->setText(dir.relativeFilePath(directory));
}

void PreferencesDialog::on_pushButton_2_clicked()
{
    close();
}

void PreferencesDialog::on_pushButton_3_clicked()
{
    ui->comboBox->setCurrentIndex(0);
    ui->comboBox_2->setCurrentIndex(0);
    ui->lineEdit->setText(QtHelpers::ExecutingDirectory() + "plugins");
}
