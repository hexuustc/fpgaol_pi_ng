#include <QCoreApplication>
#include <QObject>
#include "wsserver.h"
#include "httpserver.h"
#include "fpga.h"
#include <QtCore/QDebug>
#include <exception>
// using namespace stefanfrings;

int main(int argc, char *argv[])
{
	qInfo() << "FPGAOL_PI_NG compiled at " << __TIME__ << ", " << __DATE__;
    QCoreApplication app(argc,argv);

    app.setApplicationName("fpgaol_pi_ng");
    app.setOrganizationName("fpgaol_developer");

    new httpServer(8080, &app);
	
	wsServer *ws_server;
	FPGA *fpga;

	try {
		ws_server = new wsServer(8090, false, &app);
	} catch (const std::exception& e) {
		qWarning() << (e.what());
		return 255;
	}

	try {
		fpga = new FPGA(false);
	} catch (const std::exception& e) {
		qWarning() << (e.what());
		return 255;
	}

	QObject::connect(ws_server, &wsServer::notify_start,
			fpga, &FPGA::start_notify);
	QObject::connect(ws_server, &wsServer::notify_end,
			fpga, &FPGA::end_notify);
	QObject::connect(ws_server, &wsServer::gpio_write,
			fpga, &FPGA::write_gpio);
	QObject::connect(ws_server, &wsServer::uart_write,
			fpga, &FPGA::write_serial);
	QObject::connect(fpga, &FPGA::send_fpga_msg,
			ws_server, &wsServer::sendFPGAMessage);
	QObject::connect(fpga, &FPGA::send_uart_msg,
			ws_server, &wsServer::sendUartMessage);

    qInfo("Application has started");
    app.exec();
    qInfo("Application has stopped");
}
