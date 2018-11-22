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
#include <QFileDialog>
#include <QStorageInfo>
#include <QDirIterator>

screenshot::screenshot(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::screenshot)
{
    ui->setupUi(this);

    // читаем настройки автозапуска из файла настроек
    QSettings *settings = new QSettings("settings.conf",QSettings::IniFormat);

    bool copylastfile = settings->value("settings/copylastfile",false).toBool();

    // скрываем избыточные контролы, чтобы не перегружать форму
    if (!copylastfile) {
        ui->toolButton_2->hide();
        ui->saveDicpath_2->hide();
    }
    else {
        ui->checkBoxcopyTo->setChecked(true);
    }

    bool autorunset = settings->value("settings/autostart",false).toBool();
    ui->autostartcheckBox_3->setChecked(autorunset);

    bool ifautosortwhensave = settings->value("settings/autosort",true).toBool();
    ui->FSortcheckBox->setChecked(ifautosortwhensave);

    savedirectory = ui->saveDicpath->text();
    this->showTrayIcon();

    // настройки паузы
    ui->IntervalspinBox->setValue(settings->value("settings/interval",60).toInt());

    // формат
    ui->formatcomboBox->setCurrentIndex(ui->formatcomboBox->findText(settings->value("settings/format","png").toString()));

    // сжатие
    ui->compressspinBox->setValue(settings->value("settings/compression",-1).toInt());

    // настройки пути к скриншотам
    ui->saveDicpath->setText(settings->value("settings/directory","c:\\screens").toString());
    // настройки шаблона имени файла
    ui->filenameEdit->setText(settings->value("settings/filetem","screen").toString());

    // настройки автоматической очистки скринов
    ui->checkBoxautoclean->setChecked(settings->value("settings/autoclean",false).toBool());
    if (!ui->checkBoxautoclean->isChecked())
    {
        ui->label_7autoclean->hide();
        ui->comboBoxautoclean->hide();
    }
    else {
        // по умолчанию очистка файлов с давностью > 1 месяца
        autocleaninterval = settings->value("settings/autocleanperiod",2592000).toInt();
        ui->comboBoxautoclean->setCurrentIndex(ui->comboBoxautoclean->findText(settings->value("settings/autocleaninterval","1 месяц").toString()));
        // запуск процедуры очистки старых скринов раз в час
        cleentimer = new QTimer(this);
        connect(cleentimer,SIGNAL(timeout()),this,SLOT(clearoldscreens()));
        cleentimer->start(3600*1000);
    }

    // настройки дублирования последнего скрина в отдельную папку
    ui->saveDicpath_2->setText(settings->value("settings/copylastscreento","\\\\Out-ubuntu\\share\\share\\tmp\\test.png").toString());

    // добавлять информацию о свободном месте на дисках
    ui->checkBoxfreespace->setChecked(settings->value("settings/showfreehdspace",false).toBool());

    if (autorunset)
    {
        connect(&mtimer, SIGNAL(timeout()), this, SLOT(shootScreen()));
        mtimer.start(1000 * ui->IntervalspinBox->value());
    }
}

// очистка старых скринов
void screenshot::clearoldscreens()
{
    QDateTime tmpdatetime = QDateTime::currentDateTime();
    QDateTime limitbefore = tmpdatetime.addMonths(-1).addSecs(-autocleaninterval);

    if (ui->FSortcheckBox->isChecked())
    {
        // проверяем корректность заданной директории
        QDir dir(ui->saveDicpath->text());
        if (!dir.exists())
        {
            QMessageBox::critical(this,"Ошибка","Не существует указанная папка для сохранения скриншотов",QMessageBox::Ok);
            return;
        }
        // обыскиваем содержимое директории
        QDirIterator it(ui->saveDicpath->text(), QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile item(it.next());
            QString curfilename = it.fileName();

            curfilename = curfilename.replace("screen_" + ui->filenameEdit->text(),"");
            curfilename = curfilename.replace("." + ui->formatcomboBox->currentText(),"");
            // парсим отсюда дату
            QDateTime curdt = QDateTime::fromString(curfilename,"yyyy_MM_dd_hh_mm_ss");
            //qDebug() << curfilename << " " << curdt.isValid();
            if (curdt.isValid())
            {
                if (curdt < limitbefore)
                    item.remove();
            }
        }
        // подчищаем пустые директории
        QDirIterator folderit(ui->saveDicpath->text(), QDir::AllDirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (folderit.hasNext()) {
            QString curdirpath = folderit.next();
            QDir curdir(curdirpath);
            //qDebug() << curdirpath << " " << curdir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).count();
            if (curdir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).count() == 0)
            {
                curdir.removeRecursively();
            }
        }
    }
    else {
        // удаляем просто файлы по дате
        qDebug() << "simpledelete";
        QString workdir = ui->saveDicpath->text();
        QDir dir(workdir);
        dir.setNameFilters(QStringList() << "*." + ui->formatcomboBox->currentText());
        dir.setFilter(QDir::Files);
        foreach (QString dirFile, dir.entryList()) {
            QDateTime crfile=QFileInfo(dirFile).created();
            if (crfile < limitbefore) {
                qDebug() << dirFile;
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
            // создали новую папку - обновим путь с её учетом
            ui->saveDicpath->setText(dir.absolutePath());
        }
        else {
            delete settings;
            return;
        }

    }

    if (ui->checkBoxcopyTo->isChecked())
    {
        // copy the last screen to another folder
        copylastscreento = ui->saveDicpath_2->text();
        if (!QFileInfo::exists(copylastscreento)) {
            // такой папки не существует
            QMessageBox pmb;
            pmb.setText("Внимание!");
            pmb.setInformativeText("<b>Файл для копирования последнего скрина не существует или указан неверно. Создать такой файл?</b>");
            pmb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            int n = pmb.exec();
            if (n == QMessageBox::Yes)
            {
                // создадим такой файл
                QFile f( copylastscreento );
                f.open( QIODevice::WriteOnly );
                f.close();
            }
            else {
                ui->checkBoxcopyTo->setChecked(false);
                copylastscreento.clear();
                ui->saveDicpath_2->clear();
            }
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

    // copylastfile
    settings->setValue("settings/copylastfile",ui->checkBoxcopyTo->isChecked());

    // showfreediskspace
    settings->setValue("settings/showfreehdspace",ui->checkBoxfreespace->isChecked());

    // dir
    if (ui->saveDicpath->text().length() > 0)
    {
        settings->setValue("settings/directory",ui->saveDicpath->text());
        settings->sync();
    }

    // copyTo
    if (ui->saveDicpath_2->text().length() > 0)
    {
        settings->setValue("settings/copylastscreento",ui->saveDicpath_2->text());
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
        if (ui->comboBoxautoclean->currentText() == "2 месяца") {
            QDateTime tmp1 = QDateTime(TempDate);
            QDateTime tmp2 = tmp1.addMonths(-2);
            autocleaninterval = tmp2.secsTo(tmp1);
        }
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
    // for disk info
    if (ui->checkBoxfreespace)
        h = h + 30;
    QPixmap final(w, h);
    QPainter painter(&final);
    final.fill(Qt::black);

    // freedisk info
    QString freespaceinfo("Free disk space information: ");
    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady() && !storage.isReadOnly()) {
            // only local drives
            if (storage.device().contains("\\\\?\\") || storage.device().contains("/dev/")) {
                int totalGb = storage.bytesTotal()/(1024*1024*1024);
                int freeGb = storage.bytesFree()/(1024*1024*1024);

                float used = 100-100*(float)storage.bytesFree()/storage.bytesTotal();
                //qDebug() << storage.displayName() << QString::number(used,'f',2) << totalGb;
                freespaceinfo.append(storage.rootPath() + " " + storage.name() + " " + QString::number(used,'f',2) + "% used. (" + QString::number(totalGb - freeGb) + "/" + QString::number(totalGb) + "Gb) ");
            }
        }
    }

    //foreach (QPixmap scr, scrs) {
    //    painter.drawPixmap(QPoint(p, 0), scr);
    //    p += scr.width();
    //}

    int toY = 0;
    for (int i=scrs.length() - 1; i >= 0; i--)
    {
        if (ui->checkBoxfreespace) {
            toY += 30;
        }
        painter.drawPixmap(QPoint(p,toY),scrs[i]);
        p += scrs[i].width();
        if (ui->checkBoxfreespace) {
            painter.fillRect(0,0,p,30,Qt::white);
        }

    }
    if (ui->checkBoxfreespace) {
        painter.setFont(QFont("Arial",14));
        painter.drawText(10,5,p,30,0,freespaceinfo);
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
    // основной файл
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

    // copyTo
    // допустим полный путь к файлу верен, но файла пока не существует
    copylastscreento = ui->saveDicpath_2->text();
    QString copyToPath = copylastscreento;
    if (!QFileInfo::exists(copylastscreento))
    {
        // проверяем существует ли родительская директория
        if (QFileInfo(copylastscreento).dir().exists())
        {
            // просто пишем файл
            originalPixmap.save(copyToPath,format.toStdString().c_str(),ui->compressspinBox->value());
            return;
        }
        else {
            // директория не корректная, выходим
            return;
        }
    }
    else {
        // файл уже существует, просто перезаписываем его
        originalPixmap.save(copyToPath,format.toStdString().c_str(),ui->compressspinBox->value());
    }

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
    QString DirPath = QFileDialog::getExistingDirectory(this,tr("Укажите папку для сохранения скриншотов"),QDir::currentPath(),QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    QDir f( DirPath );
    if (f.exists())
        ui->saveDicpath->setText(DirPath);
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

void screenshot::on_toolButton_2_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,tr("Выберите файл, в который будет сохраняться копия последнего скриншота"),"",tr("PNG files (*.png);;All Files (*)"));
    QFile f( fileName );
    f.open( QIODevice::WriteOnly );
    f.close();
    ui->saveDicpath_2->setText(fileName);
}

void screenshot::on_checkBoxcopyTo_clicked()
{

}

void screenshot::on_checkBoxcopyTo_clicked(bool checked)
{
    if (checked)
    {
        ui->toolButton_2->show();
        ui->saveDicpath_2->show();
    }
    else {
        ui->toolButton_2->hide();
        ui->saveDicpath_2->hide();
    }
}
