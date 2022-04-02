#include <QGuiApplication>
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
#if (QT_VERSION > QT_VERSION_CHECK(5, 6, 0))
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icon/logo"));

    MainWindow w;
    w.show();
    return (a.exec());
}
