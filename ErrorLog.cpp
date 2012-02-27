#include "ErrorLog.h"
#include <QFile>
#include <QMessageBox>

ErrorLog::ErrorLog(QObject *parent) :
    QObject(parent)
{
    m_file = new QFile();
}

ErrorLog::~ErrorLog()
{
    delete m_file;
}

bool ErrorLog::open(const QString& filename)
{
    m_file->setFileName(filename);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(NULL, tr("�t�@�C���I�[�v���G���["), tr("�G���[�t�@�C���̃I�[�v���Ɏ��s���܂���\n%1").arg(filename));
        return false;
    } else {
        return true;
    }
}

void ErrorLog::close()
{
    m_file->close();
}

void ErrorLog::outLog(const QString& loginfo)
{
    m_file->write(loginfo);
}

