#ifndef MAINSETTINGS_H
#define MAINSETTINGS_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class MainSettings; }
QT_END_NAMESPACE

class MainSettings : public QDialog
{
    Q_OBJECT

public:
    MainSettings(QWidget *parent = nullptr);
    ~MainSettings();

private:
    Ui::MainSettings *ui;
};
#endif // MAINSETTINGS_H
