#include "httpserver.h"
#include "fpga.h"
#include <QDateTime>
#include <QIODevice>

handler::handler() {
    // Configure static file controller
    fileSettings.setValue("path", "/home/pi/docroot"); // Using absolute path when sudo
    fileSettings.setValue("encoding", "UTF-8");
    fileSettings.beginGroup("docroot");
    fileSettings.endGroup();
    staticFileController = new stefanfrings::StaticFileController(&fileSettings);
    token = TOKEN_DEBUG_IGNORE;
}

handler::~handler() {
    delete staticFileController;
}

int WriteJson(char* filename, QJsonObject json){
    QFile file(filename);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)){
        qDebug() << "File open error";
        return -1;
    }
    else{
        QTextStream out(&file);
        out << QJsonDocument(json).toJson(QJsonDocument::Compact);
        file.close();
    }
    return 0;
}

void handler::service(stefanfrings::HttpRequest& request, stefanfrings::HttpResponse& response) {
    QByteArray path=request.getPath();
    qDebug("RequestMapper: path=%s",path.data());
    QByteArray request_token = request.getParameter("token");
    qDebug("token=%s", request_token.data());
    if (path == "/") {
        if (request_token != TOKEN_DEBUG_IGNORE && request_token != token) {
            response.setStatus(403);
            response.write("invaild token(timeout maybe)", true);
            qDebug("path is in the location / , there is an invaild token");
            return;
        }
    }
    if (path == "/set/") {
        token = request_token;
        qDebug("(set)now token is:%s", token.data());
        response.setStatus(200);
        response.write("ok", true);
        return;
    }
    if (path == "/unset/") {
        token = TOKEN_DEBUG_IGNORE;
        qDebug("(unset)now token is:%s", token.data());
        response.setStatus(200);
        response.write("ok", true);
        // 当前机器被使用情况统计
        char filename[20] = "status.json";
        QFile file(filename);
        QString val;
        QJsonObject json;
        QJsonObject newjson;
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            newjson["starttime"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            newjson["usedcount"] = 1;
            WriteJson(filename, newjson);
        } else {
            val = file.readAll();
            file.close();
            QJsonDocument d = QJsonDocument::fromJson(val.toUtf8());
            json = d.object();
            int num = json["usedcount"].toInt();
            newjson["usedcount"] = num+1;
            newjson["starttime"] = json["starttime"].toString();
            WriteJson(filename, newjson);
        }
        return;
    }
    if (path == "/restart/") {
        if(request_token != TOKEN_DEBUG_IGNORE && request_token != token)
            return;
        qDebug("path is in the location /restart/");
        response.setStatus(200);
        response.write("Restarting...", true);
        gpioTerminate();
        exit(1);
    }
    if (path == "/status/") {
        response.setStatus(201);
        char filename[20] = "status.json";
        QFile file(filename);
        QString val;
        QJsonObject json;
        // if not generate new use record
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            response.write("no new status", true);
        } else {
            val = file.readAll();
            file.close();
            QJsonDocument d = QJsonDocument::fromJson(val.toUtf8());
            json = d.object();
            response.write(d.toJson(), true);
            remove(filename);
        }
    }
    if (path.startsWith("/upload"))
    {
        if (request_token != TOKEN_DEBUG_IGNORE && request_token != token) {
            response.setStatus(403);
            response.write("invaild token(timeout maybe)", true);
            qDebug("path is in the location /upload, failed, and return the 403, because the token is invalid");
            return;
        }
        QByteArray method = request.getMethod();
        response.setHeader("Access-Control-Allow-Origin", "*");
        response.setHeader("Access-Control-Allow-Headers", "x-requested-with");
        response.setHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        if (method == "POST") {
            system("rm -rf /home/pi/docroot/waveform.vcd");
            qDebug("removed useless vcd file");
            QTemporaryFile* file=request.getUploadedFile("bitstream");
            QDateTime now = QDateTime::currentDateTime();
            QString file_name = "/home/pi/bistream/bitstream.zip";
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
                    int r = FPGA::program_device();
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
