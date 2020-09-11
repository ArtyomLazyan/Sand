#include "tcpserver.h"
#include "logic.h"

#include <iostream>
//#include "logic.h"
#include <map>

#define HOST_ADDRESS "127.0.0.1" // "185.127.66.104"
#define HOST_PORT 40002
#define MAX_SOCKETS (4)

std::map<QString, int> sRequestTypeMap = {
    std::make_pair(QString("GenerateKey"), 0),
    std::make_pair(QString("Encode"), 1),
    std::make_pair(QString("Decode"), 2),
    std::make_pair(QString("Img"), 3)
};

CGameServer::CGameServer(QObject* parent):
    QTcpServer(parent)
{
    //
    /// Setup timer and connect to host
    //
    m_pFlushTimer = new QTimer(this);
    if (!listen(QHostAddress::Any, HOST_PORT))
    {
        std::cout << "Failed to listen" << std::endl;
        return;
    }
    //
    std::cout << "Listening on " << serverAddress().toString().toStdString() << ":" << HOST_PORT << std::endl;

    connect(this, &QTcpServer::newConnection, this, &CGameServer::onNewConnection);
    connect(m_pFlushTimer, &QTimer::timeout, this, &CGameServer::onFlushTimerTick);

    m_pFlushTimer->setInterval(1000);
    m_pFlushTimer->start();
    //
    /// init settings
    //
}

void CGameServer::sendMessage(QString sMessage, int user)
{
    m_users[user]->write(sMessage.toLatin1());
    m_users[user]->flush();
    m_users[user]->waitForBytesWritten(3000);
}

QString CGameServer::ToString(const std::vector<std::vector<int> >& mtxImg) const
{
    int nRowSize = mtxImg.size();
    int nColSize = mtxImg[0].size();

    QString sMatrix;
    for (int nRow = 0; nRow < nRowSize; ++nRow)
    {
        for (int nCol = 0; nCol < nColSize; ++nCol)
        {
            sMatrix += QString::number(mtxImg[nRow][nCol]);

            if (nCol < nColSize - 1)
                sMatrix += '.';
        }

        if (nRow < nRowSize - 1)
            sMatrix += '#';
    }

    return sMatrix;
}

std::vector<std::vector<int> > CGameServer::FromString(const QString& sMatrix)
{
    QStringList lstRows = sMatrix.split('#');
    std::vector<std::vector<int> > vecImg;
    vecImg.resize(lstRows.count());

    for (int nRow = 0; nRow < lstRows.count(); ++nRow)
    {
        QStringList lstCols = lstRows.at(nRow).split('.');
        vecImg[nRow].resize(lstCols.count());

        for (int nCol = 0; nCol < lstCols.count(); ++nCol)
            vecImg[nRow][nCol] = lstCols[nCol].toInt();
    }

    return vecImg;
}

void CGameServer::onNewConnection()
{
    if (m_oServerClients.size() >= m_nMaxClients)
    {
        std::cout << "Cannot form new connection. Server busy";

        QTcpSocket* pCurSocket = nextPendingConnection();
        pCurSocket->close();
        return;
    }

    QTcpSocket* pCurSocket = nextPendingConnection();
    connect(pCurSocket, &QTcpSocket::readyRead, this, &CGameServer::onReadyRead);
    connect(pCurSocket, &QTcpSocket::disconnected, this, &CGameServer::onDisconnected);

    qDebug() << "Client " << pCurSocket->socketDescriptor();
    m_oServerClients.insert(m_nUserCount, !bool(m_oServerClients.size()));
    m_users.insert(m_nUserCount, pCurSocket);

    if (0 == m_nUserCount)
    {
        //
        /// Send data to main clinet
        //
        sendMessage(QString("First"), m_nUserCount);
    }

    ++m_nUserCount;
    return;
}

void CGameServer::onReadyRead()
{
    QTcpSocket* pSocket = (QTcpSocket *)sender();
    //
    /// IF client is inactive skip data processing
    /// Do not chagne this line
    //
    int oCurrentClient = getCurrentUserID(pSocket);
    QByteArray data = pSocket->readAll();
    qDebug() << "Data from client" << data;

    QStringList sValueList = QString::fromStdString(data.toStdString()).split(",");
    QString sRequestType = sValueList[0];
    sValueList.removeAll(sRequestType);
    qDebug() << sValueList;

    switch (sRequestTypeMap[sRequestType])
    {
    case 0:
        {
            // generate key
            //sendMessage("GeneratedKey", oCurrentClient);
            qDebug() << "GenerateKey";
            if (m_pLogic != nullptr)
            {
                QString sMtxKey = ToString(m_pLogic->generateKey());
                sendMessage(QString("Key:" + sMtxKey), oCurrentClient);
            }
        }
        break;
    case 1:
        {
            // encode
            //sendMessage("EncodeData", oCurrentClient);
            qDebug() << "EncodeData";
            if (m_pLogic != nullptr)
            {
                QString sMtxImage = ToString(m_pLogic->encode());

                foreach (int user, m_users.keys())
                    sendMessage(QString("EncodedData:" + sMtxImage), user);
            }
        }
        break;
    case 2:
        {
            // decode
            //sendMessage("DecodeData", oCurrentClient);
            qDebug() << "DecodeData";

            if (m_pLogic != nullptr)
            {
                std::vector<std::vector<int> > mtxImage;
                while (!m_pLogic->getDecodedAndFinished(mtxImage))
                {
                    foreach (int user, m_users.keys())
                    {
                        QString sMtxImage = ToString(mtxImage);
                        sendMessage(QString("DecodedData:" + sMtxImage), user);
                    }
                    mtxImage.clear();
                }
            }
        }
        break;
    case 3:
        {
            std::vector<std::vector<int> > mtxImg = FromString(sValueList[0]);
            m_pLogic = new CLogicEncoder(mtxImg.size(), mtxImg[0].size());
            m_pLogic->setImage(mtxImg);
        }
        break;
    default:
        break;
    }
}

void CGameServer::onFlushTimerTick()
{
    std::flush(std::cout);
}

void CGameServer::onDisconnected()
{
    QTcpSocket* pSocket = (QTcpSocket *) sender();
    int removeUser = getCurrentUserID(pSocket);
    if (removeUser != -1)
    {
        m_oServerClients.remove(removeUser);
        m_users.remove(removeUser);
        qDebug() << "Client" << removeUser << "disconnected from game";
    }

    if (0 == m_users.size())
        cleanup();
}


int CGameServer::getNextClientIndex(QTcpSocket* pSocket)
{
    int oCurrentUser = getCurrentUserID(pSocket);
    QList<int> keys = m_users.keys();
    for(size_t i = 0; i < keys.size(); ++i)
    {
        if (keys[i] == oCurrentUser)
        {
            if (i == keys.size() - 1)
                return 0;
            else
                return i + 1;
        }
    }
}

int CGameServer::getCurrentUserID(QTcpSocket* pSocket)
{
    foreach (int user, m_users.keys())
    {
        if (pSocket == m_users[user]) return user;
    }
    return -1;
}

void CGameServer::cleanup()
{
    m_nUserCount = 0;
    m_nCount = 0;
    m_oScoreMap.clear();
    m_oServerClients.clear();
    m_oClientNamesMap.clear();
    m_bGameOver = false;

    if (0 != m_users.size())
    {
        foreach(int user, m_users.keys())
        {
            m_users[user]->close();
            delete m_users[user];
            m_users[user] = nullptr;
        }
        m_users.clear();
    }
}

