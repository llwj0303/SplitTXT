#include <QStandardPaths>
#include <QFileInfoList>
#include <QFileDialog>
#include <QDesktopServices>
#include <QMessageBox>
#include <QRegExpValidator>
#include <QTextStream>
#include <QMimeData>
#include <QFile>
#include <QDir>
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <math.h>
#include "splitterthread.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <qvalidator.h>

//extern bool bIsHasTxtHead = false;

//判断文件夹是否存在,不存在则创建
static bool isDirExist(QString fullPath)
{
    QDir dir(fullPath);
    if (dir.exists()) {
        return (true);
    }
    else {
        bool ok = dir.mkdir(fullPath);//只创建一级子目录，即必须保证上级目录存在
        return (ok);
    }
}

/**
 * @brief clearFolderFiles 仅清空文件夹内的文件(不包括子文件夹内的文件)
 * @param folderFullPath 文件夹全路径
 */
static void clearFolderFiles(const QString &folderFullPath, QString filterStr = "")
{
    QDir dir(folderFullPath);
    dir.setFilter(QDir::Files);
    int fileCount = dir.count();
    for (int i = 0; i < fileCount; i++) {
        if (!filterStr.isEmpty()) {
            if (!dir[i].contains(filterStr)) {
                continue;
            }
        }
        dir.remove(dir[i]);
    }
}

//获取选择的文件夹下所有文件集合（包括子文件夹中的文件）
static QFileInfoList GetAllFileList(QString path)
{
    QDir	  dir(path);
    QFileInfoList file_list = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoSymLinks);

    QFileInfoList folder_list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (int i = 0; i != folder_list.size(); i++) {
        QString	      name	      = folder_list.at(i).absoluteFilePath();
        QFileInfoList child_file_list = GetAllFileList(name);//递归
        file_list.append(child_file_list);
    }
    return (file_list);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_splitterThread(NULL)
{
    ui->setupUi(this);

    qApp->setOrganizationName("asy315");
    qApp->setApplicationName("AsyTxtTool");
    setWindowTitle(tr("TXT源数据处理工具"));
    this->setAcceptDrops(true);

    ui->lineEdit_IntervalLinesNum->setValidator(new QRegExpValidator(QRegExp("[0-9]+$")));

    m_totalLines = 0;
    m_desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    ui->stackedWidget->setCurrentIndex(0);

    //只能输入正整数（不含0）
    ui->lineEdit_docNum->setValidator(new QRegExpValidator(QRegExp("^([1-9][0-9]*)$")));
    ui->lineEdit_lineNum->setValidator(new QRegExpValidator(QRegExp("^([1-9][0-9]*)$")));
    ui->lineEdit_columns->setValidator(new QRegExpValidator(QRegExp("^([1-9][0-9]*)$")));
    ui->lineEdit_rows->setValidator(new QRegExpValidator(QRegExp("^([1-9][0-9]*)$")));

    ui->label_result->clear();
    ui->widget_asymmetric->setVisible(false);

    doReadSettings(getApplicationSettings());
}

MainWindow::~MainWindow()
{
    if (m_splitterThread) {
        m_splitterThread->stopImmediately();
        m_splitterThread->wait();
    }
    delete ui;
}

QSettings &MainWindow::getApplicationSettings() const
{
    static QSettings *settings = new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    return (*settings);
}

// 加载缓存配置
void MainWindow::doReadSettings(QSettings &settings)
{
    // UI elements
    settings.beginGroup("UI/SplitTxt");
    bool isSplitByLines = settings.value("isSplitByLines", true).toBool();
    if (isSplitByLines) {
        ui->specifyLinesRB->setChecked(true);
    }
    else {
        ui->averageRB->setChecked(true);
    }
    int splitLineNum = settings.value("splitLineNum", 100).toInt();
    ui->lineEdit_lineNum->setText(QString::number(splitLineNum));
    int splitDocNum = settings.value("splitDocNum", 2).toInt();
    ui->lineEdit_docNum->setText(QString::number(splitDocNum));
    settings.endGroup();

    // 文本合并
    settings.beginGroup("UI/MergeTxt");
    bool isInteralMerge = settings.value("isInteralMerge", true).toBool();
    if (isInteralMerge) {
        ui->mergeLinesRB->setChecked(true);
        int intervalLinesNum = settings.value("intervalLinesNum", 1).toInt();
        ui->lineEdit_IntervalLinesNum->setText(QString::number(intervalLinesNum));

        bool isAsymmetric = settings.value("isAsymmetric", false).toBool();
        ui->checkBox_asymmetric->setChecked(isAsymmetric);
        if (isAsymmetric) {
            int intervalLinesNum2 = settings.value("intervalLinesNum2", 1).toInt();
            ui->lineEdit_IntervalLinesNum_2->setText(QString::number(intervalLinesNum2));
        }
    }
    else {
        ui->mergeAllRB->setChecked(true);
    }
    settings.endGroup();

    // 查找字串
    settings.beginGroup("UI/SearchTxt");
    int onePdfPageNum = settings.value("onePdfPageNum", 100).toInt();
    ui->lineEdit_pages->setText(QString::number(onePdfPageNum));
    int pdfColumns = settings.value("pdfColumns", 27).toInt();
    ui->lineEdit_columns->setText(QString::number(pdfColumns));
    int pdfRows = settings.value("pdfRows", 7).toInt();
    ui->lineEdit_rows->setText(QString::number(pdfRows));
    int isVerizonFirst = settings.value("isVerizonFirst", true).toBool();
    if (isVerizonFirst) {
        ui->verizonRB->setChecked(true);
    }
    else {
        ui->horizonRB->setChecked(true);
    }
    settings.endGroup();

    // 生成序列文本
    settings.beginGroup("UI/GenerateIndexTxt");
    int totalRows = settings.value("totalRows", 10000).toInt();
    ui->lineEdit_totalRows->setText(QString::number(totalRows));
    int rowsPerPage = settings.value("rowsPerPage", 10).toInt();
    ui->lineEdit_rowsPerPage->setText(QString::number(rowsPerPage));
    int startIndex = settings.value("startIndex", 1).toInt();
    ui->lineEdit_startIndex->setText(QString::number(startIndex));
    settings.endGroup();
}

// 保存缓存配置
void MainWindow::doWriteSettings(QSettings &settings)
{
    // 文件分割
    settings.beginGroup("UI/SplitTxt");
    bool isSplitByLines = ui->specifyLinesRB->isChecked();
    settings.setValue("isSplitByLines", isSplitByLines);
    int splitLineNum = ui->lineEdit_lineNum->text().toInt();
    settings.setValue("splitLineNum", splitLineNum);
    int splitDocNum = ui->lineEdit_docNum->text().toInt();
    settings.setValue("splitDocNum", splitDocNum);
    settings.endGroup();

    // 文本合并
    settings.beginGroup("UI/MergeTxt");
    bool isInteralMerge = ui->mergeLinesRB->isChecked();
    settings.setValue("isInteralMerge", isInteralMerge);
    int intervalLinesNum = ui->lineEdit_IntervalLinesNum->text().toInt();
    settings.setValue("intervalLinesNum", intervalLinesNum);
    bool isAsymmetric = ui->checkBox_asymmetric->isChecked();
    settings.setValue("isAsymmetric", isAsymmetric);
    int intervalLinesNum2 = ui->lineEdit_IntervalLinesNum_2->text().toInt();
    settings.setValue("intervalLinesNum2", intervalLinesNum2);
    settings.endGroup();

    // 字符查询
    settings.beginGroup("UI/SearchTxt");
    int onePdfPageNum = ui->lineEdit_pages->text().toInt();
    settings.setValue("onePdfPageNum", onePdfPageNum);
    int pdfColumns = ui->lineEdit_columns->text().toInt();
    settings.setValue("pdfColumns", pdfColumns);
    int pdfRows = ui->lineEdit_rows->text().toInt();
    settings.setValue("pdfRows", pdfRows);
    bool isVerizonFirst = ui->verizonRB->isChecked();
    settings.setValue("isVerizonFirst", isVerizonFirst);
    settings.endGroup();

    // 生成序列文本
    settings.beginGroup("UI/GenerateIndexTxt");
    int totalRows = ui->lineEdit_totalRows->text().toInt();
    settings.setValue("totalRows", totalRows);
    int rowsPerPage = ui->lineEdit_rowsPerPage->text().toInt();
    settings.setValue("rowsPerPage", rowsPerPage);
    int startIndex = ui->lineEdit_startIndex->text().toInt();
    settings.setValue("startIndex", startIndex);
    settings.endGroup();
}

void MainWindow::on_openFileBtn_clicked()
{
    QStringList fileNameList = QFileDialog::getOpenFileNames(this,
                                                             tr("Open Txt"),
                                                             m_desktopDir,
                                                             tr("Txt files (*.txt)"));
    if (!fileNameList.isEmpty()) {
        if (fileNameList.count() == 1 && ui->stackedWidget->currentIndex() == 0) {
            ui->stackedWidget->setCurrentIndex(0);
            ui->startBtn->setText(tr("开始分割"));

            ui->lineEdit_txt->setText(fileNameList.at(0));
            ui->startBtn->setEnabled(true);

            m_totalLines = calcTxtTotalLines(ui->lineEdit_txt->text());
            QTime stopTime = QTime::currentTime();
            long  elapsed  = m_startTime0.msecsTo(stopTime);
            qDebug("calcTxtTotalLines use time: %ld ms", elapsed);

            if (m_totalLines == 0 || m_totalLines == -1) {
                QMessageBox::warning(this, tr("警告"), tr("该TXT文件为空或无法打开！"));
                ui->lineEdit_txt->clear();
                return;
            }
            else {
                statusBar()->showMessage(tr("  文件共%1行").arg(m_totalLines));
            }
        }
        else if (fileNameList.count() == 1 && ui->stackedWidget->currentIndex() == 3) {
            ui->stackedWidget->setCurrentIndex(3);
            ui->startBtn->setText(tr("开始生成"));

            ui->lineEdit_txt->setText(fileNameList.at(0));
            ui->startBtn->setEnabled(true);

            m_totalLines = calcTxtTotalLines(ui->lineEdit_txt->text());
            QTime stopTime = QTime::currentTime();
            long  elapsed  = m_startTime0.msecsTo(stopTime);
            qDebug("calcTxtTotalLines use time: %ld ms", elapsed);

            if (m_totalLines == 0 || m_totalLines == -1) {
                QMessageBox::warning(this, tr("警告"), tr("该TXT文件为空或无法打开！"));
                ui->lineEdit_txt->clear();
                return;
            }
            else {
                ui->lineEdit_totalRows->setText(QString::number(m_totalLines));
                statusBar()->showMessage(tr("  文件共%1行").arg(m_totalLines));
            }
        }
        else if (fileNameList.count() > 1) {
            on_switchComboBox_currentIndexChanged(1);

            QString mergeStr;
            ui->textEdit->clear();
            for (int i = 0; i < fileNameList.count(); ++i) {
                mergeStr.append(fileNameList.at(i));
                ui->textEdit->append(fileNameList.at(i));
                if (i != fileNameList.count() - 1) {
                    mergeStr.append(";");
                }
            }
            ui->lineEdit_txt->setText(mergeStr);

            statusBar()->showMessage(tr("  共%1个文件").arg(fileNameList.count()));
        }
    }
}

void MainWindow::on_startBtn_clicked()
{
    QString inputFile = ui->lineEdit_txt->text();
    if ((inputFile.isEmpty() || !inputFile.endsWith(".txt")) && (ui->stackedWidget->currentIndex() != 3)) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择要操作的txt文件！"));
        return;
    }

    ui->startBtn->setEnabled(false);
    // 文件分割
    if (ui->stackedWidget->currentIndex() == 0) {
        m_createDateTime = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
        int linesCount;
        if (ui->specifyLinesRB->isChecked()) {//按行数分割
            linesCount = ui->lineEdit_lineNum->text().toInt();
        }
        else {// 平均分割
              //计算txt总行数，向前进位
            float docNum      = ui->lineEdit_docNum->text().toFloat();
            int	  perDocLines = ceil(m_totalLines / docNum);
            linesCount = perDocLines == 0 ? 1 : perDocLines;
        }

        m_outDir = QApplication::applicationDirPath() + "/txtout/" + m_createDateTime + "/";
        isDirExist(m_outDir);
        if (m_outDir.trimmed().length() == 0) {
            QString   inDir = ui->lineEdit_txt->text();
            QFileInfo info(inDir);
            m_outDir = info.absolutePath();
        }

        m_outDir = formatPath(m_outDir);
        m_outDir.replace(QRegExp("/$"), "");
        m_outDir += "/";
        initOutputTxtDirs(m_outDir);

        QFileInfo info(inputFile);
        QString	  outFilename = info.baseName();

        QString	   outputFullPathStr = m_outDir + outFilename;
        QByteArray inputPathData, outputPathData;
        inputPathData = inputFile.toLocal8Bit();
        string intputFileString = string(inputPathData);
        outputPathData = outputFullPathStr.toLocal8Bit();
        string outputFileString = string(outputPathData);

        m_splitterThread = new SplitterThread(NULL);
        m_splitterThread->setInputFile(intputFileString);
        m_splitterThread->setOutputFile(outputFileString);
        m_splitterThread->setLinesCount(linesCount);

        connect(m_splitterThread, SIGNAL(finished()), this, SLOT(splitThreadFinished()));
        connect(m_splitterThread, &QThread::finished, m_splitterThread, &QObject::deleteLater);

        if (m_splitterThread->isRunning()) {
            return;
        }
        m_splitterThread->start();
        m_startTime = QTime::currentTime();
    }
    else if (ui->stackedWidget->currentIndex() == 1) { // 文件合并
        QStringList inputFiles = ui->textEdit->toPlainText().split("\n");
        qDebug() << inputFiles;
        m_startTime2	 = QTime::currentTime();
        m_createDateTime = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
        m_outDir	 = QApplication::applicationDirPath() + "/txtout/" + m_createDateTime + "/";
        isDirExist(m_outDir);
        m_outDir = formatPath(m_outDir);
        m_outDir.replace(QRegExp("/$"), "");
        m_outDir += "/";
        initOutputTxtDirs(m_outDir);
        QString outFilename	  = "mergeTxt.txt";
        QString outputFullPathStr = m_outDir + outFilename;
        if (inputFiles.count() > 4 || inputFiles.count() < 2) {
            QMessageBox::warning(this, tr("提示"), tr("请选择2~4个txt文件！"));
            ui->startBtn->setEnabled(true);
            return;
        }

        int intervalLinesNum  = ui->lineEdit_IntervalLinesNum->text().toInt();
        int intervalLinesNum2 = ui->lineEdit_IntervalLinesNum_2->text().toInt();
        mergeTxtFiles(inputFiles, outputFullPathStr, intervalLinesNum, intervalLinesNum2);
        ui->startBtn->setEnabled(true);

        QTime stopTime = QTime::currentTime();
        long  elapsed  = m_startTime2.msecsTo(stopTime);
        statusBar()->showMessage(tr("  合并耗时%1s").arg(elapsed * 0.001));
    }
    else if (ui->stackedWidget->currentIndex() == 2) { // 字串查找
        ui->label_result->clear();
        QString searchStr = ui->lineEdit_searchStr->text();
        if (searchStr.isEmpty()) {
            QMessageBox::warning(this, tr("提示"), tr("请先输入要查询的字串！"));
            ui->startBtn->setEnabled(true);
            return;
        }
        else {
            int lineIndex = getLineNumInTxt(searchStr);

            int	 page		= ui->lineEdit_pages->text().toInt();
            int	 row		= ui->lineEdit_rows->text().toInt();
            int	 column		= ui->lineEdit_columns->text().toInt();
            int	 onePageCount	= row * column;
            bool isVerizonFirst = ui->verizonRB->isChecked();// 默认纵向优先

            if (lineIndex == -1) {
                ui->label_result->setText(tr("未搜索到结果，请重试！"));
                ui->startBtn->setEnabled(true);
            }
            else {
                qreal tempNum = lineIndex * 1.0 / onePageCount;
                // pdf文件序号索引
                int fileIndex = ceil(tempNum / page);
                // 页码索引
                int pageIndex = ceil(tempNum - (fileIndex - 1) * page);

                // 小数部分
                qreal decimalNum = tempNum - floor(tempNum);
                // 所在页行索引
                int rowIndex	= 0;
                int columnIndex = 0;  // 所在页列索引

                if (isVerizonFirst) { // 纵向优先
                    columnIndex = ceil(decimalNum * onePageCount / row);
                    rowIndex	= ceil(decimalNum * onePageCount - (columnIndex - 1) * row);
                }
                else {// 横向优先
                    rowIndex	= ceil(decimalNum * onePageCount / column);
                    columnIndex = ceil(decimalNum * onePageCount - (rowIndex - 1) * column);
                }

                QString fileIndexFix = QString("%1").arg(fileIndex, 3, 10, QLatin1Char('0'));
                ui->label_result->setText(tr("查找到该字串在编号<font color = red><b> %1 </b></font>的PDF文件，"
                                             "第<font color = red><b> %2 </b></font>页，"
                                             "<font color = red><b> %3 </b></font>行 "
                                             "<font color = red><b> %4 </b></font>列")
                                             .arg(fileIndexFix)
                                             .arg(pageIndex)
                                             .arg(rowIndex)
                                             .arg(columnIndex));
                ui->startBtn->setEnabled(true);
            }
            QTime stopTime = QTime::currentTime();
            long  elapsed  = m_startTime2.msecsTo(stopTime);
            statusBar()->showMessage(tr("  查询耗时%1s").arg(elapsed * 0.001));
        }
    }
    else if (ui->stackedWidget->currentIndex() == 3) { // 生成序列文本
        m_createDateTime = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
        int linesCount;
        linesCount = ui->lineEdit_totalRows->text().toInt();

        m_outDir = QApplication::applicationDirPath() + "/txtout/" + m_createDateTime + "/";
        isDirExist(m_outDir);
        if (m_outDir.trimmed().length() == 0) {
            QString   inDir = ui->lineEdit_txt->text();
            QFileInfo info(inDir);
            m_outDir = info.absolutePath();
        }

        m_outDir = formatPath(m_outDir);
        m_outDir.replace(QRegExp("/$"), "");
        m_outDir += "/";
        initOutputTxtDirs(m_outDir);

        QString outFilename;
        if (inputFile.isEmpty() || !inputFile.endsWith(".txt")) {
            outFilename = "序列号文本_" + m_createDateTime + ".txt";
        }
        else {
            QFileInfo info(inputFile);
            outFilename = info.baseName();
        }

        QString outputFullPathStr = m_outDir + outFilename;
        generateSerialIndexTxt(outputFullPathStr);
    }
}

QString MainWindow::formatPath(QString path)
{
    QString   res = "";
    QFileInfo info(path);
    res = info.absoluteFilePath();
    res.replace(QRegExp("/$"), "");
    return (res);
}

void MainWindow::initOutputTxtDirs(QString path)
{
    QDir d;
    d.mkpath(path);
    clearFolderFiles(path);
}

void MainWindow::splitThreadFinished()
{
    QTime stopTime = QTime::currentTime();
    long  elapsed  = m_startTime.msecsTo(stopTime);
    qDebug("SplitTxtThread use time: %ld ms", elapsed);

    QFileInfoList list = GetAllFileList(QApplication::applicationDirPath() + "/txtout/" + m_createDateTime + "/");
    statusBar()->showMessage(tr("  文件共%1行，分割成%2个文件，耗时%3s").arg(m_totalLines).arg(list.count()).arg(elapsed / 1000.0));
    ui->startBtn->setEnabled(true);
}

void MainWindow::on_openDir_clicked()
{
    isDirExist(m_outDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_outDir));
}

int MainWindow::calcTxtTotalLines(QString textFilePath)
{
    qDebug() << " textFilePath " << textFilePath;

    m_startTime0 = QTime::currentTime();
    QString showInfo(tr("正在计算文件行数..."));
    statusBar()->showMessage(showInfo);

    QFile file(textFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return (-1);
    }
    int totalLines = 0;
    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        QString	   line_in(line);
        if (!line_in.isEmpty()) { // 通用
            totalLines++;
        }
//        if((!line_in.isEmpty()) && line_in.contains("://"))
//        {
//            totalLines++;
//        }
        QCoreApplication::processEvents(); // 防止界面假死
    }

    return (totalLines);
}

int MainWindow::getLineNumInTxt(QString searchStr)
{
    int targetIndex = -1;

    m_startTime2 = QTime::currentTime();
    QString showInfo(tr("正在查询，请稍候..."));
    statusBar()->showMessage(showInfo);

    QFile file(ui->lineEdit_txt->text());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return (-1);
    }

    int totalLines = 0;
    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        QString	   line_in(line);
        if (!line_in.isEmpty()) { // 通用
            totalLines++;

            if (line_in.contains(searchStr)) {
                targetIndex = totalLines;
                break;
            }
        }
        QCoreApplication::processEvents(); // 防止界面假死
    }

    return (targetIndex);
}

void MainWindow::mergeTxtFiles(QStringList fileList, QString outFilePath,
                               int intervalLinesNum, int intervalLinesNum2)
{
    QFile outFile(outFilePath);
    outFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream aStream(&outFile);

    QByteArray	tempData1; //最多可存储2147483647个字节 (< 2G)
    QByteArray	tempData2;
    QByteArray	tempData3;
    QByteArray	tempData4;
    QStringList line_all1;
    QStringList line_all2;
    QStringList line_all3;
    QStringList line_all4;

    // 间隔1行，交错插入
    QFile inputFile1, inputFile2, inputFile3, inputFile4;

    inputFile1.setFileName(fileList.at(0));
    inputFile1.open(QIODevice::ReadOnly | QIODevice::Text);
    tempData1 = inputFile1.readAll();
    line_all1 = QString(tempData1).split("\n");
    inputFile1.close();

    if (fileList.count() > 1) {
        inputFile2.setFileName(fileList.at(1));
        inputFile2.open(QIODevice::ReadOnly | QIODevice::Text);
        tempData2 = inputFile2.readAll();
        line_all2 = QString(tempData2).split("\n");
        inputFile2.close();
    }

    if (fileList.count() > 2) {
        inputFile3.setFileName(fileList.at(2));
        inputFile3.open(QIODevice::ReadOnly | QIODevice::Text);
        tempData3 = inputFile3.readAll();
        line_all3 = QString(tempData3).split("\n");
        inputFile3.close();
    }

    if (fileList.count() > 3) {
        inputFile4.setFileName(fileList.at(3));
        inputFile4.open(QIODevice::ReadOnly | QIODevice::Text);
        tempData4 = inputFile4.readAll();
        line_all4 = QString(tempData4).split("\n");
        inputFile4.close();
    }

    if (ui->mergeLinesRB->isChecked()) {             // 间隔指定行数合并
        if (!ui->checkBox_asymmetric->isChecked()) { // 对称交替合并
            for (int i = 0; i < line_all1.count(); i += intervalLinesNum) {
                for (int j1 = 0; j1 < intervalLinesNum; ++j1) {
                    if (line_all1.count() > i + j1) {
                        aStream << line_all1.at(i + j1);
                    }
                    aStream << "\n";
                }

                if (fileList.count() > 1 && line_all2.count() > i) {
                    for (int j2 = 0; j2 < intervalLinesNum; ++j2) {
                        if (line_all2.count() > i + j2) {
                            aStream << line_all2.at(i + j2);
                        }
                        aStream << "\n";
                    }
                }

                if (fileList.count() > 2 && line_all3.count() > i) {
                    for (int j3 = 0; j3 < intervalLinesNum; ++j3) {
                        if (line_all3.count() > i + j3) {
                            aStream << line_all3.at(i + j3);
                        }
                        aStream << "\n";
                    }
                }

                if (fileList.count() > 3 && line_all4.count() > i) {
                    for (int j4 = 0; j4 < intervalLinesNum; ++j4) {
                        if (line_all4.count() > i + j4) {
                            aStream << line_all4.at(i + j4);
                        }
                        aStream << "\n";
                    }
                }
                QCoreApplication::processEvents(); // 防止界面假死
            }
        }
        else { // 对称交替合并 只支持两个文件合并
            if ((line_all1.count() * 1.0 / intervalLinesNum) >= (line_all2.count() * 1.0 / intervalLinesNum2)) {
                for (int i = 0; i < line_all1.count(); i += intervalLinesNum) {
                    for (int j1 = 0; j1 < intervalLinesNum; ++j1) {
                        if (line_all1.count() > i + j1) {
                            aStream << line_all1.at(i + j1);
                        }
                        aStream << "\n";
                    }

                    if (fileList.count() > 1) {
                        int mIndex = i / intervalLinesNum * intervalLinesNum2;
                        for (int j2 = 0; j2 < intervalLinesNum2; ++j2) {
                            if (line_all2.count() > mIndex + j2) {
                                aStream << line_all2.at(mIndex + j2);
                            }
                            aStream << "\n";
                        }
                    }
                }
            }
            else { // 文件2需循环更多次
                for (int i = 0; i < line_all2.count(); i += intervalLinesNum2) {
                    int mIndex = i / intervalLinesNum2 * intervalLinesNum;
                    for (int j1 = 0; j1 < intervalLinesNum; ++j1) {
                        if (line_all1.count() > mIndex + j1) {
                            aStream << line_all1.at(mIndex + j1);
                        }
                        aStream << "\n";
                    }

                    if (fileList.count() > 1) {
                        for (int j2 = 0; j2 < intervalLinesNum2; ++j2) {
                            if (line_all2.count() > i + j2) {
                                aStream << line_all2.at(i + j2);
                            }
                            aStream << "\n";
                        }
                    }
                }
            }
            QCoreApplication::processEvents(); // 防止界面假死
        }
    }
    else if (ui->mergeAllRB->isChecked()) { // 衔尾合并
        aStream << tempData1;
        aStream << "\n";

        if (fileList.count() > 1) {
            aStream << tempData2;
            aStream << "\n";
        }

        if (fileList.count() > 2) {
            aStream << tempData3;
            aStream << "\n";
        }

        if (fileList.count() > 3) {
            aStream << tempData4;
            aStream << "\n";
        }
        QCoreApplication::processEvents(); // 防止界面假死
    }

    outFile.close();
}

void MainWindow::generateSerialIndexTxt(QString fileName)
{
    QFile outFile(fileName);
    outFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream aStream(&outFile);

    QString startIndexStr = ui->lineEdit_startIndex->text();
    int	    startIndex	  = startIndexStr.isEmpty() ? 1 : startIndexStr.toInt();
    int	    interalCount  = ui->lineEdit_rowsPerPage->text().toInt();
    int	    totalRowsNum  = ui->lineEdit_totalRows->text().toInt();

    int rowNum = 0;

    do {
        for (int index = 0; index < interalCount; ++index) {
            aStream << QString::number(startIndex);
            aStream << "\n";
            rowNum++;
        }
        if (rowNum % interalCount == 0) {
            startIndex++;
        }
    }while (rowNum < totalRowsNum);

    outFile.close();

    statusBar()->showMessage(tr(" Txt文件生成成功"));
    ui->startBtn->setEnabled(true);
}

void MainWindow::closeEvent(QCloseEvent */*event*/)
{
    QSettings &settings = getApplicationSettings();
    doWriteSettings(settings);
    settings.sync();

    this->close();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList <QUrl> urls = event->mimeData()->urls();

    QString fileNames;
    for (int index = 0; index < urls.count(); ++index) {
        fileNames.append(urls.at(index).toLocalFile());
        ui->textEdit->append(urls.at(index).toLocalFile());
        if (index != urls.count() - 1) {
            fileNames.append(";");
        }
    }
    ui->lineEdit_txt->setText(fileNames);
    ui->startBtn->setEnabled(true);

    // 除合并文本外，计算Txt总行数
    if (ui->stackedWidget->currentIndex() != 1) {
        m_totalLines = calcTxtTotalLines(ui->lineEdit_txt->text());
        QTime stopTime = QTime::currentTime();
        long  elapsed  = m_startTime0.msecsTo(stopTime);
        qDebug("calcTxtTotalLines use time: %ld ms", elapsed);
        if (m_totalLines == 0 || m_totalLines == -1) {
            QMessageBox::warning(this, tr("警告"), tr("该TXT文件为空或无法打开！"));
            ui->lineEdit_txt->clear();
            return;
        }
        else {
            if (ui->stackedWidget->currentIndex() == 3) {
                ui->lineEdit_totalRows->setText(QString::number(m_totalLines));
            }
            statusBar()->showMessage(tr("  文件共%1行").arg(m_totalLines));
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *ev)
{
    if (ev->mimeData()->hasFormat("text/uri-list")) {
        ev->acceptProposedAction();
    }
}

void MainWindow::on_lineEdit_lineNum_textEdited(const QString &text)
{
    if (!ui->specifyLinesRB->isChecked()) {
        ui->specifyLinesRB->setChecked(true);
    }
    if (text.toInt() > m_totalLines && m_totalLines != 0) {
        QMessageBox::warning(this, tr("警告"), tr("分割行数不能大于TXT总行数"));
        ui->lineEdit_lineNum->setText("100");
        return;
    }
    ui->lineEdit_lineNum->setText(text);
}

void MainWindow::on_lineEdit_docNum_textEdited(const QString &text)
{
    if (!ui->averageRB->isChecked()) {
        ui->averageRB->setChecked(true);
    }
    if (text.toInt() > m_totalLines && m_totalLines != 0) {
        QMessageBox::warning(this, tr("警告"), tr("分割份数不能大于TXT总行数"));
        ui->lineEdit_docNum->setText("2");
        return;
    }
    ui->lineEdit_docNum->setText(text);
}

void MainWindow::on_switchComboBox_currentIndexChanged(int index)
{
    ui->stackedWidget->setCurrentIndex(index);
    switch (index) {
    case 0: // Txt分割
    {
        ui->switchComboBox->setCurrentText(tr("Txt分割"));
        ui->startBtn->setText(tr("开始分割"));
        ui->openDir->setVisible(true);
    }
    break;

    case 1: // Txt合并
    {
        ui->switchComboBox->setCurrentText(tr("Txt合并"));
        ui->startBtn->setText(tr("开始合并"));
        ui->openDir->setVisible(true);
    }
    break;

    case 2: // 查找字串
    {
        ui->switchComboBox->setCurrentText(tr("查找字串"));
        ui->startBtn->setText(tr("开始查找"));
        ui->openDir->setVisible(false);
    }
    break;

    case 3: // 生成序列文本
    {
        ui->switchComboBox->setCurrentText(tr("生成序列文本"));
        ui->startBtn->setText(tr("开始生成"));
    }
    break;

    default:
        break;
    }
}

void MainWindow::on_lineEdit_IntervalLinesNum_textEdited(const QString &text)
{
    m_intervalLinesNum = text.toInt();
    ui->lineEdit_IntervalLinesNum->setText(text);
}

void MainWindow::on_checkBox_asymmetric_stateChanged(int state)
{
    ui->widget_asymmetric->setVisible(state);
}
