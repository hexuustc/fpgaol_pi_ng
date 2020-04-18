#ifndef FPGA_H
#define FPGA_H

#include <string>
#include <thread>
#include <QSerialPort>
#include <QByteArray>
#include <QWebSocket>

class FPGA
{
private:
	// TODO: Do we really need two websockets?
	QWebSocket *gpio_ws;
	QWebSocket *serial_ws;
	QSerialPort serial_port;

	// Don't need SoftClockThread any more, pigpio can help us do so
	std::thread monitor_thrd;
public:

	// Instantiated upon successfully open WebSocket connections
	FPGA(QWebSocket *_gpio_ws, QWebSocket *_serial_ws);

	// Called when WebSocket connection is closed
	~FPGA();

	// Program the FPGA, give me the absolute path
	// Returns: return code of `fjtgcfg`, 0 if successful
	int program_device(std::string filename);

	// Write a certain gpio
	// Returns: return code of `pigpio.write`, 0 if successful
	int write_gpio(int gpio, int level);

	// Write to the serial
	// Returns: Number of bytes written
	int write_serial(QByteArray msg);

	// Set soft clock at certain frequecy, will use `gpioWaveTxSend`
	// Returns: 0 if successful
	int set_soft_clock(int freq_hz);
};

#endif /* FPGA_H */
