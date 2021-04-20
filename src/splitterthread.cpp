#include "splitterthread.h"
#include <Windows.h>
#include <tchar.h>
#include <vector>
#include <string>
#include <QString>

#pragma warning(disable:4996)
using namespace std;

SplitterThread::SplitterThread(QObject *parent) :
    QThread(parent)
{
}

void SplitterThread::run() {
    std::ifstream textFile(inputFile.c_str(), ios::binary | ios::in);
    if (!textFile.is_open()) {
        qFatal("SplitterThread cannot open file");
        return;
    }

    int enc = ENC_UTF8;//默认UTF8

    char buf[2];
    textFile.read(buf, 2);
    unsigned char b0 = (unsigned char) buf[0];
    unsigned char b1 = (unsigned char) buf[1];

    if ((b0 == 0xFF && b1 == 0xFE) || (b0 != 0x00 && b1 == 0x00)) {
        enc = ENC_UTF16_LE;
    }
    else if ((b0 == 0xFE && b1 == 0xFF) || (b0 == 0x00 && b1 != 0x00)) {
        enc = ENC_UTF16_BE;
    }
    textFile.seekg(0);

    stopThread = false;
    bool finish = false;
    int txtNum = 1;
    int lines = 0;

    char c0;

    while(textFile.peek() != EOF) {
        QMutexLocker locker(&m_lock);
        if (stopThread)
            return;

        lines = 0;
        QString txtIndexStr = "";
        txtIndexStr = txtIndexStr.sprintf("%03d", txtNum++);
        string outputFullFileName = outputFile + "_" + txtIndexStr.toStdString() + ".txt";
        fstream outFile(outputFullFileName.c_str(), ios::binary | ios::out);
        if (!outFile.is_open()) {
            qFatal("outputTxtFile cannot open file");
            return;
        }

        //第一个文件txtNum++后是2
        if(txtNum == 2 && 0/*ConfigApp::bIsHasTxtHead*/)
        {
            realLinesCount = linesCount + 1;
        }else{
            realLinesCount = linesCount;
        }

        while (lines < realLinesCount) {
            textFile.read(&c0, 1);
            outFile.write(&c0, 1);

            //修复txt分割最后一行内容被改变问题 to solve last line text error
            if (textFile.peek() == EOF) {
                if (lines == 0) {
                    outFile.close();
                    remove(outputFullFileName.c_str());
                }
                finish = true;
                break;
            }

            if (c0 == 0x0a) {
                if (enc == ENC_UTF16_LE) {
                    textFile.read(&c0, 1);
                    outFile.write(&c0, 1);
                }

                lines++;
            }
        }

        if (finish) break;
        outFile.close();
    }

    textFile.close();
}

void SplitterThread::stopImmediately()
{
    QMutexLocker locker(&m_lock);
    stopThread = true;
}
