#include <QCoreApplication>
#include "global.h"
#include "httplistener.h"
#include "requestmapper.h"
#include "util.h"

using namespace stefanfrings;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc,argv);

    app.setApplicationName("fpgaol_pi_ng");
    app.setOrganizationName("fpgaol_developer");

    // Find the configuration file
    QString configFileName=searchConfigFile();

    // Configure logging into a file
    QSettings* logSettings=new QSettings(configFileName,QSettings::IniFormat,&app);
    logSettings->beginGroup("logging");
    FileLogger* logger=new FileLogger(logSettings,10000,&app);
    logger->installMsgHandler();

    // Configure static file controller
    QSettings* fileSettings=new QSettings(configFileName,QSettings::IniFormat,&app);
    fileSettings->beginGroup("docroot");
    staticFileController=new StaticFileController(fileSettings,&app);

    // Configure and start the TCP listener
    QSettings* listenerSettings=new QSettings(configFileName,QSettings::IniFormat,&app);
    listenerSettings->beginGroup("listener");
    new HttpListener(listenerSettings,new RequestMapper(&app),&app);

    qInfo("Application has started");
    app.exec();
    qInfo("Application has stopped");
}
