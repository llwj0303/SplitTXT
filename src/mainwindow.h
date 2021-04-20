#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTime>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class SplitterThread;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QString formatPath(QString path);
    void initOutputTxtDirs(QString path);
    int calcTxtTotalLines(QString textFilePath);

private slots:
    void splitThreadFinished();

    void on_openFileBtn_clicked();

    void on_startSplitBtn_clicked();

    void on_openURL_clicked();

    void on_lineEdit_lineNum_textEdited(const QString &arg1);

    void on_lineEdit_docNum_textEdited(const QString &arg1);

private:
    Ui::MainWindow *ui;

    QTime            m_startTime;
    QString          m_outDir;

    SplitterThread  *m_splitterThread;
    QString          m_desktopDir;

    QString          m_createDateTime;
    int              m_totalLines;
};
#endif // MAINWINDOW_H
