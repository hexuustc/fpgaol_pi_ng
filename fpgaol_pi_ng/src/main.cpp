#include <QCoreApplication>
#include "wsserver.h"
#include "httpserver.h"
#include "util.h"
#include <QtCore/QDebug>
using namespace stefanfrings;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc,argv);

    app.setApplicationName("fpgaol_pi_ng");
    app.setOrganizationName("fpgaol_developer");

    new httpServer(8080, &app);
    new wsServer(8090, true, &app);

    qInfo("Application has started");
    app.exec();
    qInfo("Application has stopped");
}
