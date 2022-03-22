#ifndef WSSERVER_H
#define WSSERVER_H
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

class wsServer : public QObject
{
    Q_OBJECT
public:
    explicit wsServer(quint16 port, bool debug = false, QObject *parent = nullptr);
    ~wsServer();

public Q_SLOTS:
    void sendFPGAMessage(QString message);
Q_SIGNALS:
	int notify_start(QString msg);
	int notify_end();
	int gpio_write(int gpio, int level);
	int uart_write(QByteArray msg);

    void closed();

private Q_SLOTS:
    void onNewConnection();

    void recvFPGAMessage(QString message);

    void processBinaryMessage(QByteArray message);
    void FPGASocketDisconnected();

private:
    QWebSocketServer *m_pWebSocketServer;
    QList<QWebSocket *> FPGA_clients;
    bool m_debug;
};
#endif // WSSERVER_H
