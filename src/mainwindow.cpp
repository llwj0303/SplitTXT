#include <QStandardPaths>
#include <QFileInfoList>
#include <QFileDialog>
#include <QDesktopServices>
#include <QMessageBox>
#include <QRegExpValidator>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <math.h>
#include "splitterthread.h"

//extern bool bIsHasTxtHead = false;

//判断文件夹是否存在,不存在则创建
static bool isDirExist(QString fullPath)
{
    QDir dir(fullPath);
    if(dir.exists())
    {
      return true;
    }
    else
    {
       bool ok = dir.mkdir(fullPath);//只创建一级子目录，即必须保证上级目录存在
       return ok;
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
    for (int i = 0; i < fileCount; i++)
    {
        if(!filterStr.isEmpty())
        {
            if(!dir[i].contains(filterStr))
                continue;
        }
        dir.remove(dir[i]);
    }
}

//获取选择的文件夹下所有文件集合（包括子文件夹中的文件）
static QFileInfoList GetAllFileList(QString path)
{
    QDir dir(path);
    QFileInfoList file_list = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoSymLinks);

    QFileInfoList folder_list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(int i = 0; i != folder_list.size(); i++)
    {
        QString name = folder_list.at(i).absoluteFilePath();
        QFileInfoList child_file_list = GetAllFileList(name);//递归
        file_list.append(child_file_list);
    }
    return file_list;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_splitterThread(NULL)
{
    ui->setupUi(this);
    setWindowTitle(tr("TXT文本分割工具"));

    m_totalLines = 0;
    m_desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    //只能输入正整数（不含0）
    ui->lineEdit_docNum->setValidator(new QRegExpValidator(QRegExp("^([1-9][0-9]*)$")));
    ui->lineEdit_lineNum->setValidator(new QRegExpValidator(QRegExp("^([1-9][0-9]*)$")));
}

MainWindow::~MainWindow()
{
    m_splitterThread->stopImmediately();
    m_splitterThread->wait();
    delete ui;
}

void MainWindow::on_openFileBtn_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open Txt"),
                                                    m_desktopDir,
                                                    tr("Txt files (*.txt)"));
    if(!fileName.isEmpty())
    {
        ui->lineEdit_txt->setText(fileName);
        ui->startSplitBtn->setEnabled(true);

        m_totalLines = calcTxtTotalLines(ui->lineEdit_txt->text());
        if(m_totalLines == 0 || m_totalLines == -1)
        {
            QMessageBox::warning(this, tr("警告"), tr("该TXT文件为空或无法打开！"));
            ui->lineEdit_txt->clear();
            return;
        }
    }
}

void MainWindow::on_startSplitBtn_clicked()
{
    QString inputFile = ui->lineEdit_txt->text();
    if(inputFile.isEmpty() || !inputFile.endsWith(".txt"))
    {
        QMessageBox::warning(this, tr("警告"), tr("请先选择要分割的txt文件！"));
        return;
    }

    ui->startSplitBtn->setEnabled(false);
    m_createDateTime = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
    int linesCount;
    if(ui->specifyLinesRB->isChecked())//按行数分割
    {
        linesCount = ui->lineEdit_lineNum->text().toInt();
    }
    else if(ui->averageRB->isChecked())//平均分割
    {
        //计算txt总行数，向前进位
        float docNum = ui->lineEdit_docNum->text().toFloat();
        int perDocLines = ceil(m_totalLines/docNum);
        linesCount = perDocLines == 0 ? 1 : perDocLines;
    }

    m_outDir = QApplication::applicationDirPath() + "/txtout/" + m_createDateTime + "/";
    isDirExist(m_outDir);
    if (m_outDir.trimmed().length() == 0) {
        QString inDir = ui->lineEdit_txt->text();
        QFileInfo info(inDir);
        m_outDir = info.absolutePath();
    }

    m_outDir = formatPath(m_outDir);
    m_outDir.replace(QRegExp("/$"), "");
    m_outDir += "/";
    initOutputTxtDirs(m_outDir);

    QString outFilename = "splitTxt";
    if (outFilename.trimmed().length() == 0) {
        QFileInfo info(inputFile);
        outFilename = info.fileName();
    }

    QString outputFullPathStr = m_outDir + outFilename;
    QByteArray inputPathData,outputPathData;
    inputPathData = inputFile.toLocal8Bit();
    string intputFileString = string(inputPathData);
    outputPathData = outputFullPathStr.toLocal8Bit();
    string outputFileString = string(outputPathData);

    m_splitterThread = new SplitterThread(NULL);
    m_splitterThread->setInputFile(intputFileString);
    m_splitterThread->setOutputFile(outputFileString);
    m_splitterThread->setLinesCount(linesCount);

    connect(m_splitterThread, SIGNAL(finished()), this, SLOT(splitThreadFinished()) );
    connect(m_splitterThread, &QThread::finished, m_splitterThread, &QObject::deleteLater);

    if(m_splitterThread->isRunning())
        return;
    m_splitterThread->start();
    m_startTime = QTime::currentTime();
}

QString MainWindow::formatPath(QString path) {
    QString res = "";
    QFileInfo info(path);
    res = info.absoluteFilePath();
    res.replace(QRegExp("/$"), "");
    return res;
}

void MainWindow::initOutputTxtDirs(QString path) {
    QDir d;
    d.mkpath(path);
    clearFolderFiles(path);
}

void MainWindow::splitThreadFinished()
{
    QTime stopTime = QTime::currentTime();
    int elapsed = m_startTime.msecsTo(stopTime);
    qDebug("SplitTxtThread use time: %ld ms", elapsed);

    QFileInfoList list = GetAllFileList(QApplication::applicationDirPath() + "/txtout/" + m_createDateTime + "/");
    statusBar()->showMessage(tr("文件共%1行，分割成%2个文件，耗时%3s").arg(m_totalLines).arg(list.count()).arg(elapsed/1000.0));
    ui->startSplitBtn->setEnabled(true);
}

void MainWindow::on_openURL_clicked()
{
    isDirExist(m_outDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_outDir));
}

int MainWindow::calcTxtTotalLines(QString textFilePath)
{
    qDebug()<<" textFilePath "<<textFilePath;
    int totalLines = 0;

    QFile file(textFilePath);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
//        QMessageBox::warning(this, tr("警告"), tr("无法打开TXT文件!"));
        return -1;
    }

    while(!file.atEnd())
    {
        QByteArray line = file.readLine();
        QString line_in(line);
        if(!line_in.isEmpty()) // 通用
        {
            totalLines++;
        }

//        if((!line_in.isEmpty()) && line_in.contains("://"))
//        {
//            totalLines++;
//        }
    }
    return totalLines;
}

void MainWindow::on_lineEdit_lineNum_textEdited(const QString &text)
{
    if(!ui->specifyLinesRB->isChecked())
        ui->specifyLinesRB->setChecked(true);
    if(text.toInt() > m_totalLines && m_totalLines != 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("分割行数不能大于TXT总行数"));
        ui->lineEdit_lineNum->setText("100");
        return;
    }
    ui->lineEdit_lineNum->setText(text);
}

void MainWindow::on_lineEdit_docNum_textEdited(const QString &text)
{
    if(!ui->averageRB->isChecked())
        ui->averageRB->setChecked(true);
    if(text.toInt() > m_totalLines && m_totalLines != 0)
    {
        QMessageBox::warning(this, tr("警告"), tr("分割份数不能大于TXT总行数"));
        ui->lineEdit_docNum->setText("2");
        return;
    }
    ui->lineEdit_docNum->setText(text);
}
