#include "tcpclient.h"
#include <iostream>
#include <QMessageBox>

#define HOST_ADDRESS "127.0.0.1"
#define HOST_PORT 40001

std::map<QString, int> sRequestTypeMap = {
    std::make_pair(QString("First"), 0),
    std::make_pair(QString("Key"), 1),
    std::make_pair(QString("EncodedData"), 2),
    std::make_pair(QString("DecodedData"), 3)
};

CClient::CClient()
{
    //
    /// Init socket and timer
    //
    m_pSocket = new QTcpSocket(this);
    m_pFlushTimer = new QTimer(this);

    //
    /// Make connections
    //
    connect(m_pSocket, &QTcpSocket::connected, this, &CClient::onSocketConnected);
    connect(m_pSocket, &QTcpSocket::readyRead, this, &CClient::onReadyRead);
    connect(m_pSocket, &QTcpSocket::disconnected, this, &CClient::onSocketDisconnected);
    connect(m_pFlushTimer, &QTimer::timeout, this, &CClient::onFlushTimerTick);

    //
    /// Flush data to std::out
    //
    m_pFlushTimer->setInterval(1000);
    m_pFlushTimer->start();
}

bool CClient::isConnected()
{
    return m_pSocket->state() == QAbstractSocket::ConnectedState;
}

void CClient::connectToServer()
{
    if (m_pSocket->state() == QAbstractSocket::UnconnectedState)
    {
        m_pSocket->connectToHost(HOST_ADDRESS, HOST_PORT);
        bool bConnected = m_pSocket->waitForConnected(3000);
        if (!bConnected)
        {
           (void)QMessageBox::critical(nullptr, tr("COVID-19"), tr(m_pSocket->errorString().toStdString().c_str()), QMessageBox::Close);
           exit(0);
        }
    }
}

void CClient::sendMessage(QString msg)
{
    QString msgToSend = msg;
    if (m_pSocket->state() != QAbstractSocket::ConnectedState)
    {
        return;
    }

    QByteArray data = msgToSend.toLatin1();
    m_pSocket->write(data);
    m_pSocket->waitForBytesWritten(3000);
}

void CClient::onEncode()
{

}

void CClient::onDecode()
{

}

void CClient::onGenerateKey()
{

}

void CClient::disconnectFromServer()
{
    qDebug() << "state: " << m_pSocket->state();
    if (m_pSocket->state() != QAbstractSocket::ConnectedState)
    {
        return;
    }

    qDebug() << "Client disconnecting";
    m_pSocket->disconnectFromHost();
}

void CClient::onSocketConnected()
{
    onReadyRead();
}

void CClient::onSocketDisconnected()
{
}

void CClient::onReadyRead()
{
    qDebug() << "Reading...";
    QByteArray data = m_pSocket->readAll();
    qDebug() << "data from server " << data;
    if (data.isEmpty())
    {
        qDebug() << "Dat is empty";
        return;
    }

    QStringList sValueList = QString::fromStdString(data.toStdString()).split(":");
    QString sRequestType = sValueList[0];
    sValueList.removeAll(sRequestType);

    qDebug() << sValueList;

    switch (sRequestTypeMap[sRequestType])
    {
    case 0:
        {
            qDebug() << "First";
            emit sigFirst();
        }
        break;
    case 1:
        {
            qDebug() << "Key";
            emit sigKeyGenerated(sValueList[0]);
       }
        break;
    case 2:
        {
            qDebug() << "EncodedData";
            emit sigEncodedData(sValueList[0]);
        }
        break;
    case 3:
        {
            qDebug() << "DecodedData";
            emit sigDecodedData(sValueList[0]);
        }
        break;
    default:
        break;
    }
}

void CClient::onFlushTimerTick()
{
    std::flush(std::cout);
}


