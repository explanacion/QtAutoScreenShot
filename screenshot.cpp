#include "screenshot.h"
#include "ui_screenshot.h"
#include <QSettings>
#include <QMessageBox>
#include <QDate>
#include <QTime>
#include <QPixmap>
#include <QDir>
#include <QDesktopServices>
#include <QScreen>
#include <QUrl>
#include <QIcon>
#include <QDebug>
#include <QPainter>

screenshot::screenshot(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::screenshot)
{
    ui->setupUi(this);

    // читаем настройки автозапуска из файла настроек
    QSettings *settings = new QSettings("settings.conf",QSettings::IniFormat);
    bool autorunset = settings->value("settings/autostart",false).toBool();
    ui->autostartcheckBox_3->setChecked(autorunset);

    bool ifautosortwhensave = settings->value("settings/autosort",true).toBool();
    ui->FSortcheckBox->setChecked(ifautosortwhensave);

    savedirectory = ui->saveDicpath->text();
    this->showTrayIcon();
    if (ui->autostartcheckBox_3->isChecked())
    {
        connect(&mtimer, SIGNAL(timeout()), this, SLOT(shootScreen()));
        mtimer.start(1000 * ui->IntervalspinBox->value());
    }

    // формат
    ui->formatcomboBox->setCurrentIndex(ui->formatcomboBox->findText(settings->value("settings/format","png").toString()));

    // сжатие
    ui->compressspinBox->setValue(settings->value("settings/compression",-1).toInt());

    // настройки пути к скриншотам
    ui->saveDicpath->setText(settings->value("settings/directory","c:\\screens").toString());
    // настройки шаблона имени файла
    ui->filenameEdit->setText(settings->value("settings/filetem","screen").toString());
    // настройки паузы
    ui->IntervalspinBox->setValue(settings->value("settings/interval",60).toInt());
    // настройки автоматической очистки скринов
    ui->checkBoxautoclean->setChecked(settings->value("settings/autoclean",false).toBool());
    if (!ui->checkBoxautoclean->isChecked())
    {
        ui->label_7autoclean->hide();
        ui->comboBoxautoclean->hide();
    }
    else {
        autocleaninterval = settings->value("settings/autocleanperiod",2592000).toInt();
        ui->comboBoxautoclean->setCurrentIndex(ui->comboBoxautoclean->findText(settings->value("settings/autocleaninterval","1 месяц").toString()));
        // запуск процедуры очистки старых скринов раз в сутки
        cleentimer = new QTimer(this);
        connect(cleentimer,SIGNAL(timeout()),this,SLOT(clearoldscreens()));
        cleentimer->start(86400);
    }
}

// очистка старых скринов
void screenshot::clearoldscreens()
{
    QDateTime tmpdatetime = QDateTime::currentDateTime();
    QDateTime limitbefore = tmpdatetime.addMonths(-1).addSecs(-autocleaninterval);

    if (ui->FSortcheckBox->isChecked())
    {
        // удаляем папки по месяцам
        int n = tmpdatetime.toString("MM").toInt();
        int l = limitbefore.toString("MM").toInt();
        QString nyear = tmpdatetime.toString("yyyy");
        QString lyear = limitbefore.toString("yyyy");
        tmpdatetime=tmpdatetime.addMonths(-1);
        qDebug() << tmpdatetime.toString("MM");
        // сначала удаляем папки, созданные в этом году
        n = n - 1; // предыдущий месяц
        while (n > 0) {
            if (n <= l) {
                // qDebug() << l << " " << n;
                QString workpath = ui->saveDicpath->text() + "\\" + nyear;
                QDir tempdir(workpath + "\\" + tmpdatetime.toString("MM"));
                if (tempdir.exists()) {
                    tempdir.removeRecursively();
                }
            }
            tmpdatetime=tmpdatetime.addMonths(-1);
            n--;
        }
        // теперь переходим к предыдущему году
        if (lyear.toInt() < nyear.toInt()) {
            qDebug() << "prevyear";
            QString workpath = ui->saveDicpath->text() + "\\" + lyear;
            QDir tempdir(workpath + "\\" + lyear);
            if (tempdir.exists())
            {
                // определяем подпапки предыдущего года
                // начальная точка - конец предыдущего года
                tmpdatetime = QDateTime::fromString(nyear + "-01-01 00:00:00","yyyy-MM-dd hh:mm:ss");
                tmpdatetime=tmpdatetime.addSecs(-1);
                while (tmpdatetime > limitbefore) {
                    QString curmonth = tmpdatetime.toString("MM");
                    QDir tempdir(workpath + "\\" + lyear + "\\" + curmonth);
                    if (tempdir.exists())
                    {
                        tempdir.removeRecursively();
                    }
                    tmpdatetime=tmpdatetime.addMonths(-1);
                }
            }
        }
    }
    else {
        // удаляем просто файлы по дате
        QString workdir = ui->saveDicpath->text();
        QDir dir(workdir);
        dir.setNameFilters(QStringList() << "*." + ui->formatcomboBox->currentText());
        dir.setFilter(QDir::Files);
        foreach (QString dirFile, dir.entryList()) {
            QDateTime crfile=QFileInfo(dirFile).created();
            if (crfile < limitbefore) {
                dir.remove(dirFile);
            }
        }
    }
}

screenshot::~screenshot()
{
    delete ui;
}

void screenshot::on_pushButton_clicked()
{
    // Применяем выбранные настройки
    QSettings *settings = new QSettings("settings.conf",QSettings::IniFormat);
    savedirectory = ui->saveDicpath->text();
    if (!QDir(savedirectory).exists())
    {
        // такой папки не существует
        QMessageBox pmb;
        pmb.setText("Внимание!");
        pmb.setInformativeText("<b>Папка для сохранения не существует или указана неверно. Создать такую папку?</b>");
        pmb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        int n = pmb.exec();
        if (n == QMessageBox::Yes)
        {
            // создадим папку
            QDir dir = QDir::root();
            dir.mkdir(ui->saveDicpath->text());
        }
        else {
            delete settings;
            return;
        }

    }

    // compress
    settings->setValue("settings/compression",ui->compressspinBox->value());

    // format
    settings->setValue("settings/format",ui->formatcomboBox->currentText());

    // interval
    settings->setValue("settings/interval",ui->IntervalspinBox->value());


    // autosort
    settings->setValue("settings/autosort",ui->FSortcheckBox->isChecked());

    // dir
    if (ui->saveDicpath->text().length() > 0)
    {
        settings->setValue("settings/directory",ui->saveDicpath->text());
        settings->sync();
    }

    // file
    if (ui->filenameEdit->text().length() > 0)
    {
        settings->setValue("settings/filetem",ui->filenameEdit->text());
        settings->sync();
    }


    savefilename = ui->filenameEdit->text();

    QDate cdate = QDate::currentDate();
    QTime ctime = QTime::currentTime();
    QDateTime nowdatetime = QDateTime(cdate,ctime,Qt::LocalTime);
    QString strdatestamp = nowdatetime.toString("yyyy-MM-dd hh:mm:ss");
    savefilename = savefilename + strdatestamp;

    if (ui->autostartcheckBox_3->isChecked())
    {
        settings->setValue("settings/autostart","true");
        settings->sync();
    }
    else {
        settings->setValue("settings/autostart","false");
        settings->sync();
    }

    // autoclean
    settings->setValue("settings/autoclean",ui->checkBoxautoclean->isChecked());
    if (ui->checkBoxautoclean->isChecked()) {
        QDate TempDate = QDate::currentDate();
        TempDate=TempDate.addMonths(-1);
        if (ui->comboBoxautoclean->currentText() == "1 месяц") autocleaninterval=TempDate.daysInMonth()*24*3600;
        if (ui->comboBoxautoclean->currentText() == "3 месяца") {
            QDateTime tmp1 = QDateTime(TempDate);
            QDateTime tmp2 = tmp1.addMonths(-3);
            autocleaninterval = tmp2.secsTo(tmp1);
        }
        if (ui->comboBoxautoclean->currentText() == "6 месяцев") {
            QDateTime tmp1 = QDateTime(TempDate);
            QDateTime tmp2 = tmp1.addMonths(-6);
            autocleaninterval = tmp2.secsTo(tmp1);
        }
        if (ui->comboBoxautoclean->currentText() == "1 год") {
            QDateTime tmp1 = QDateTime(TempDate);
            QDateTime tmp2 = tmp1.addYears(-1);
            autocleaninterval = tmp2.secsTo(tmp1);
        }

        settings->setValue("settings/autocleanperiod",autocleaninterval);
        settings->setValue("settings/autocleaninterval",ui->comboBoxautoclean->currentText());
    }

    shootScreen(); // сразу сделать снимок

    // инициализация таймера если он еще не стартанул автозапуском
    if (!mtimer.isActive()) {
            connect(&mtimer, SIGNAL(timeout()), this, SLOT(shootScreen()));
            mtimer.start(1000 * ui->IntervalspinBox->value());
    }
    delete settings;

}

void screenshot::showTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    QIcon trayImage(":/images/icon.png");
    trayIcon->setIcon(trayImage);
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon->show();
}

void screenshot::trayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    switch (reason)
    {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            if (this->isHidden())
            {
                this->showNormal();
            }
            else
            {
                this->hide();
            }
            break;
        default:
            break;
    }
}

void screenshot::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event -> type() == QEvent::WindowStateChange)
    {
        if (isMinimized())
        {
            this -> hide();
        }
    }
}

// функция создания скрина
void screenshot::shootScreen()
{
    QList<QScreen*> screens = QGuiApplication::screens();
    QList<QPixmap> scrs;
    int w = 0, h = 0, p = 0;
    foreach (QScreen *scr, screens) {
        QRect xr = scr->geometry();
        int x = xr.x();
        int y = xr.y();
        QPixmap pix = scr->grabWindow(0,x,y,xr.width(),xr.height());
        w += pix.width();
        if (h < pix.height()) h = pix.height();
        scrs << pix;
    }
    QPixmap final(w, h);
    QPainter painter(&final);
    final.fill(Qt::black);


    //foreach (QPixmap scr, scrs) {
    //    painter.drawPixmap(QPoint(p, 0), scr);
    //    p += scr.width();
    //}

    for (int i=scrs.length() - 1; i >= 0; i--)
    {
        painter.drawPixmap(QPoint(p,0),scrs[i]);
        p += scrs[i].width();
    }

    QPixmap originalPixmap = final;

    // save
    QString format = ui->formatcomboBox->currentText();
    QString initialPath = ui->saveDicpath->text() + tr("\\screen_");
    QDate cdate = QDate::currentDate();
    QTime ctime = QTime::currentTime();
    QDateTime nowdatetime = QDateTime(cdate,ctime,Qt::LocalTime);
    QString strdatestamp = nowdatetime.toString("yyyy_MM_dd_hh_mm_ss");
    savefilename = ui->filenameEdit->text() + strdatestamp + "." + format;
    // autosort
    if (ui->FSortcheckBox->isChecked())
    {
        initialPath = ui->saveDicpath->text() + "\\" + nowdatetime.toString("yyyy") + "\\" + nowdatetime.toString("MM") + "\\" + nowdatetime.toString("dd") + "\\";
    }
    QString fileName = initialPath + "screen_" + savefilename;
    if (!QDir(initialPath).exists()) {
        // если папки нет - создаем её
        QDir dir = QDir::root();
        dir.mkpath(initialPath);
    }
    if (!originalPixmap.save(fileName, format.toStdString().c_str(),ui->compressspinBox->value()))
    {
        QMessageBox pmb;
        pmb.setText("Внимание!");
        pmb.setInformativeText("Произошла ошибка при сохранении скриншота на диск.");
        int n = pmb.exec();
    }

}

void screenshot::on_addnumcheckBox_clicked()
{
}



void screenshot::on_datatimecheckBox_2_clicked()
{
}

void screenshot::on_pushButton_2_clicked()
{
    mtimer.stop();
}

void screenshot::on_pushButton_3_clicked()
{
    QDesktopServices::openUrl(QUrl(QUrl::fromLocalFile(savedirectory)));
}

void screenshot::on_compresslSlider_valueChanged(int value)
{
    ui->compressspinBox->setValue(ui->compresslSlider->value());
}

void screenshot::on_compressspinBox_valueChanged(int arg1)
{
    ui->compresslSlider->setValue(ui->compressspinBox->value());
}

void screenshot::on_toolButton_clicked()
{
    // save file dialog
}

void screenshot::on_checkBoxautoclean_clicked()
{
    if (!ui->checkBoxautoclean->isChecked()) {
        ui->comboBoxautoclean->hide();
        ui->label_7autoclean->hide();
    }
    else {
        ui->comboBoxautoclean->show();
        ui->label_7autoclean->show();
    }
}
