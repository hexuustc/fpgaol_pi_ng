#ifndef HTTPSERVER_H
#define HTTPSERVER_H
#include "httplistener.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "httprequesthandler.h"
#include "staticfilecontroller.h"
#include <QtCore/QObject>

class handler : public stefanfrings::HttpRequestHandler {
    Q_OBJECT
    Q_DISABLE_COPY(handler)
private:
    QSettings fileSettings;
    stefanfrings::StaticFileController* staticFileController;
public:
    handler();
    ~handler();
    void service(stefanfrings::HttpRequest& request, stefanfrings::HttpResponse& response);
};

class httpServer : public QObject
{
    Q_OBJECT
public:
    explicit httpServer(quint16 port, QObject *parent = nullptr);
    ~httpServer();
private:
    QSettings listenerSettings;
    stefanfrings::HttpListener* httpListener;
    stefanfrings::HttpRequestHandler* httpHandler;
};

#endif // HTTPSERVER_H
