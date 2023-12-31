#include "wsserver.h"
#include "QtWebSockets/qwebsocketserver.h"
#include "QtWebSockets/qwebsocket.h"
#include <QtCore/QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QJsonDocument>
#include <exception>
#include <iostream>
#include <string>

#include "periph.h"
#include "peripherals.h"

QT_USE_NAMESPACE

wsServer::wsServer(quint16 port, bool debug, QObject *parent) : QObject(parent),
	m_pWebSocketServer(new QWebSocketServer(QStringLiteral("FPAGOL WebSocket Server"),
											QWebSocketServer::NonSecureMode, this)),
	m_debug(debug)
{
    if (m_pWebSocketServer->listen(QHostAddress::Any, port))
    {
        if (m_debug)
            qDebug() << "FPAGOL WebSocket Server listening on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &wsServer::onNewConnection);
        connect(m_pWebSocketServer, &QWebSocketServer::closed, this, &wsServer::closed);
    }
    else
    {
        throw std::runtime_error("Ws server failed to start!");
    }
}

wsServer::~wsServer()
{
    m_pWebSocketServer->close();
    qDebug() << "wsServer DISCONNECTED!";
    // qDeleteAll(uart_clients.begin(), uart_clients.end());
    qDeleteAll(FPGA_clients.begin(), FPGA_clients.end());
}

void wsServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

    QString req_url = pSocket->requestUrl().toString();

    if (req_url.endsWith("/ws/"))
    {
        qDebug() << "FPGA ws connected";
        connect(pSocket, &QWebSocket::textMessageReceived, this, &wsServer::recvFPGAMessage);
        connect(pSocket, &QWebSocket::binaryMessageReceived, this, &wsServer::processBinaryMessage);
        connect(pSocket, &QWebSocket::disconnected, this, &wsServer::FPGASocketDisconnected);
        FPGA_clients << pSocket;
    }
}

// from hardware to frontend
void wsServer::sendFPGAMessage(QString message)
{
    for (QWebSocket *pClinet : FPGA_clients)
    {
        pClinet->sendTextMessage(message);
    }
}

// from frontend webpage to hardware
void wsServer::recvFPGAMessage(QString message)
{
    qDebug() <<"wsServer::recvFPGAMessage";
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        auto json = QJsonDocument::fromJson(message.toUtf8());

		if (m_debug)
            qDebug() << "FPGA Message received: ID: " << json["id"] << " level: " << json["level"];

		int id = json["id"].toInt();

        int level;
        QString msg;

        int ret;
        if (id == -1)
        {
			// FINISH
            qDebug() << "END notify";
            if (emit notify_end() == 0)
            {
				QJsonObject reply;
				//QJsonArray _v;
				level = (int)json["level"].toBool();
				if (level == 0)
				{
					qDebug() << "Level: " << json["level"].toBool();
					reply["type"] = "WF";
					//reply["values"] = _v;
					auto msg = QJsonDocument(reply).toJson(QJsonDocument::Compact);

					qDebug() << "Send: " << msg;

					pClient->sendTextMessage(msg);
				}
            }
        }
        else if (id == -2)
        {
			// INIT
			// parse the init json that contains customized peripheral information
			// after Periph initialization and GPIO allocation, the XDC constraint
			// (or equivalent json, or an error) is return to frontend
            //int inputn = json["inputn"].toInt();
            //int outputn = json["outputn"].toInt();
            //int segn = json["segn"].toInt();
            ret = emit notify_start(message);
            if (m_debug)
                qDebug() << "Start Notify returned: " << ret;
        }
        else if (id == -3)
        {
			// ordinary notification message
			// find the corresponding peripheral and notify it
			std::string type = json["type"].toString().toStdString();
			int idx = json["idx"].toInt();
			qDebug() << "MSG from " << type.c_str() << idx;
			std::map<std::string, int>::iterator itr = periphstr2id_map.find(type);
			if (itr == periphstr2id_map.end()) {
				qDebug() << "ERR: peripheral type " << type.c_str() << " invliad!";
				return;
			}
			int type_id = itr->second;
			// on the interface we'd better pay more attention to corner cases
			// as frontend users might be hostile
			std::map<int, std::vector<Periph*> >::iterator itr2 = periph_arr.find(type_id);
			if (itr2 == periph_arr.end()) {
				qDebug() << "ERR: there is no type " << type.c_str() << " peripheral!";
				return;
			}
			std::vector<Periph*>&v = itr2->second;
			//ret = v[idx]->on_notify(message);
			for (int i = 0; i < v.size(); i++) {
				if (v[i]->idx == idx) {
				// message is customized with (maybe) unique entries, 
				// so only plausible way is to pass the msg(or json) itself
				// TODO: consider using emit and signal here
				// only continuous index starting from 0 is tested!!!
				// we expect msg from frontend is human-issued, so 
				// performance not a concern here. 
					ret = v[i]->on_notify(message);
					if (ret)
						qDebug() << "WARN: non-zero return on peripheral " \
							<< type.c_str() << ":" << idx << " notify";
				}
			}
				//qDebug() << "ERR: peripheral " << type.c_str() << idx << " index out of range!";
        }
        else
        {
            qDebug() << "WARNING: reach undefined place!!!!!!!!!!!!!!!";
        }
        // pClient->sendTextMessage(message);
    }
}

void wsServer::processBinaryMessage(QByteArray message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (m_debug)
        qDebug() << "Binary Message received:" << message;
    if (pClient)
    {
        pClient->sendBinaryMessage(message);
    }
}

void wsServer::FPGASocketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (m_debug)
        qDebug() << "FPGA socketDisconnected:" << pClient;
    if (pClient)
    {
        emit notify_end();

        FPGA_clients.removeAll(pClient);
        pClient->deleteLater();
    }
}
