#ifndef FPGA_H
#define FPGA_H

#include <string>
#include <thread>
#include <QSerialPort>
#include <QByteArray>
#include <QString>
#include <QWebSocket>

class FPGA : public QObject
{
	Q_OBJECT

private:
	// TODO: Do we really need two websockets?
	QSerialPort serial_port;
	bool m_debug;

	// Don't need SoftClockThread any more, pigpio can help us do so
	std::thread monitor_thrd;
	std::thread uart_thrd;
public:

	// Instantiated upon successfully open WebSocket connections
	FPGA(bool m_debug=false);

	// Called when WebSocket connection is closed
	~FPGA();

	void call_send_fpga_msg(QString msg);
	void call_send_uart_msg(QString msg);

	static int program_device();
public slots:
	int start_notify();

	int end_notify();

	// Write a certain gpio
	// Returns: return code of `pigpio.write`, 0 if successful
	int write_gpio(int gpio, int level);

	// Write to the serial
	// Returns: Number of bytes written
	int write_serial(QByteArray msg);

	// Set soft clock at certain frequecy, will use `gpioWaveTxSend`
	// Returns: 0 if successful
	int set_soft_clock(int freq_hz);

signals:
	void send_fpga_msg(QString msg);

	void send_uart_msg(QString msg);
};

#endif /* FPGA_H */
