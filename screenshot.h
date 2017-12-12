#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <QWidget>
#include <QTimer>
#include <QSystemTrayIcon>

namespace Ui {
class screenshot;
}

class screenshot : public QWidget
{
    Q_OBJECT

public:
    explicit screenshot(QWidget *parent = 0);
    QString savefilename;
    QString savedirectory;
    QString copylastscreento;
    int inteval;
    QTimer mtimer;
    QAction *minimizeAction;
    QAction *restoreAction;
    QAction *quitAction;
    QSystemTrayIcon *trayIcon;
    qint64 autocleaninterval;
    ~screenshot();

private slots:
    void shootScreen();
    void showTrayIcon();
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void changeEvent(QEvent*);

    void on_pushButton_clicked();

    void on_addnumcheckBox_clicked();

    void on_datatimecheckBox_2_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void on_compresslSlider_valueChanged(int value);

    void on_compressspinBox_valueChanged(int arg1);

    void on_toolButton_clicked();

    void on_checkBoxautoclean_clicked();

    void clearoldscreens();

    void on_toolButton_2_clicked();

    void on_checkBoxcopyTo_clicked();

    void on_checkBoxcopyTo_clicked(bool checked);

private:
    void createOptionsGroupBox();
    void createButtonsLayout();
    QTimer *cleentimer;

    Ui::screenshot *ui;
};

#endif // SCREENSHOT_H
