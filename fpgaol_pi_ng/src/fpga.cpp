#include <pigpio.h>
#include "fpga.h"

FPGA::FPGA(QWebSocket *_gpio_ws, QWebSocket *_serial_ws) : gpio_ws(_gpio_ws), serial_ws(_serial_ws) {
	// TODO: Initialize serial port
}

FPGA::~FPGA() {

}

int FPGA::program_device(std::string filename) {
	return 0;
}

int FPGA::write_gpio(int gpio, int level) {
	return 0;
}

int FPGA::write_serial(QByteArray msg) {
	return 0;
}

int FPGA::set_soft_clock(int freq_hz) {
	return 0;
}
