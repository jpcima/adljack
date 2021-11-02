#include "main_settings.h"
#include "./ui_main_settings.h"

MainSettings::MainSettings(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MainSettings)
{
    ui->setupUi(this);
}

MainSettings::~MainSettings()
{
    delete ui;
}

