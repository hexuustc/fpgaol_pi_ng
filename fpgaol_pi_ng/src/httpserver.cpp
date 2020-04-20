#include "httpserver.h"
#include "util.h"
#include <QDateTime>
#include <QIODevice>

handler::handler() {
    // Configure static file controller

    fileSettings.setValue("path", "../../docroot");
    fileSettings.setValue("encoding", "UTF-8");
    fileSettings.beginGroup("docroot");
    fileSettings.endGroup();
    staticFileController = new stefanfrings::StaticFileController(&fileSettings);
}

handler::~handler() {
    delete staticFileController;
}

void handler::service(stefanfrings::HttpRequest& request, stefanfrings::HttpResponse& response) {
    QByteArray path=request.getPath();
    qDebug("RequestMapper: path=%s",path.data());
    if (path.startsWith("/upload"))
    {
        QByteArray method = request.getMethod();
        response.setHeader("Access-Control-Allow-Origin", "*");
        response.setHeader("Access-Control-Allow-Headers", "x-requested-with");
        response.setHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        if (method == "POST") {
            QTemporaryFile* file=request.getUploadedFile("bitstream");
            QDateTime now = QDateTime::currentDateTime();
            QString file_name = "../../" + now.toString("yyyyMMddHHmmss") + ".bit";
            QFile file_save(file_name);
            if (!file_save.open((QIODevice::WriteOnly))) {
                qDebug("save bitstream file failed");
                return;
            }
            if (file)
            {
                while (!file->atEnd() && !file->error())
                {
                    QByteArray buffer=file->read(65536);
                    file_save.write(buffer);
                }
                if (file->atEnd()) {
                    file_save.close();
                    int r = programFPGA(file_name);
                    if (r) {
                        qDebug() << "program failed, ret" << r;
                        response.setStatus(500);
                        response.write("Program failed", true);
                    }
                } else {
                    qDebug("upload file error");
                    response.setStatus(500);
                    response.write("upload failed", true);
                }
                response.setStatus(200);
                response.write("Program success!", true);
            }
            else
            {
                response.setStatus(500);
                response.write("upload failed", true);
            }
        } else if (method == "OPTIONS") {
            response.setStatus(204);
            response.write("", true);
        }
    }
    else
    {
        staticFileController->service(request, response);
    }

    qDebug("RequestMapper: finished request");
}

httpServer::httpServer(quint16 port, QObject *parent) : QObject(parent) {

    listenerSettings.setValue("port", port);
    listenerSettings.setValue("minThreads", 4);
    listenerSettings.setValue("maxThreads", 20);
    listenerSettings.setValue("cleanupInterval", 60000);
    listenerSettings.setValue("readTimeout", 60000);
    listenerSettings.setValue("maxRequestSize", 16000);
    listenerSettings.setValue("maxMultiPartSize", 10000000);
    listenerSettings.beginGroup("listener");
    listenerSettings.endGroup();
    httpHandler = new handler();
    httpListener = new stefanfrings::HttpListener(&listenerSettings, httpHandler);
}

httpServer::~httpServer() {
    delete httpListener;
    delete httpHandler;
}
