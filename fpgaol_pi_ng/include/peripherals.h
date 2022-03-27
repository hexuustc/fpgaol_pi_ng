/**
 * File              : peripherals.h
 * License           : GPL-3.0-or-later
 * Author            : FPGAOL
 * Date              : 2022.03.12
 * Last Modified Date: 2022.03.12
 */
#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include "fpga.h"
#include "periph.h"

#define MAX_PERIPH_COUNT 100
#define MAX_KIND_OF_PERIPH 100

extern std::map<int, std::vector<Periph*> > periph_arr;
extern std::map<int, std::string> periphid2str_map;
extern std::map<std::string, int> periphstr2id_map;
extern std::map<int, int> pincnt_map;
extern std::map<int, int> needpoll_map;

extern FPGA* fpga_instance;

#define DUMMY_ID 1000

#define LED_ID 1001
class LED : public Periph
{
	private:
		// why int? because we need PWM driving LED lumination
		int light;
	public:
		LED(int type, int idx, bool needpoll, int pincnt, int pins[]) :
			Periph(type, idx, needpoll, pincnt, pins) { }
		void send_msg() {
			QJsonObject json;
			//std::cout << type << " " << idx << " " << pins[0] << std::endl;
			std::string s = periphid2str_map.find(type)->second;
			json["id"] = 0;
			json["type"] = QString::fromStdString(s);
			json["idx"] = idx;
			json["payload"] = light;
			auto msg = QJsonDocument(json).toJson(QJsonDocument::Compact);
			fpga_instance->call_send_fpga_msg(msg);
		}
		virtual int poll() override {
			int curlight = gpioRead(pins[0]);
			//qDebug() << "Poll LED";
			if (curlight != light) {
				light = curlight;
				send_msg();
			}
			return 0;
		};
		//int on_notify(QString msg) { return 0; }
};

#define BTN_ID 1002
// buttons and switches are the same, so-called press-release
// is implemented by frontend
class BTN : public Periph
{
	private:
		bool curstate;
	public:
		BTN(int type, int idx, bool needpoll, int pincnt, int pins[]) :
			Periph(type, idx, needpoll, pincnt, pins) { }
		virtual int on_notify(QString msg) override {
			auto json = QJsonDocument::fromJson(msg.toUtf8());
			int onoff = (int)json["payload"].toBool();
			gpioWrite(pins[0], onoff);
			//std::cout << "BTN" << " " << pins[0] << " " <<  idx << " " << onoff << std::endl;
			return 0;
		};
};

#define UART_ID 1003
class UART : public Periph
{
	// idx -- /dev/serialX
	private:
	int baudrate;
	int serport;
	public:
		UART(int type, int idx, bool needpoll, int pincnt, int pins[], int baud) :
			Periph(type, idx, needpoll, pincnt, pins) {
				baudrate = baud;
				char serstr[20] = "/dev/serial0";
				serstr[11] += idx;
				serport = serOpen(serstr, baudrate, 0);
			}
		~UART() {
			serClose(serport);
		}
		virtual int on_notify(QString msg) override {
			auto json = QJsonDocument::fromJson(msg.toUtf8());
			std::string s = json["payload"].toString().toStdString();
			serWrite(serport, (char*)s.c_str(), s.length());
			std::cout << "UART IN" << std::endl;
			//std::cout << "BTN" << " " << pins[0] << " " <<  idx << " " << onoff << std::endl;
			return 0;
		}
		virtual int poll() override {
			char data[1000];
			int available_data = serDataAvailable(serport);
			if (available_data) {
				serRead(serport, data, available_data);
				data[available_data] = '\0';
				QJsonObject json;
				json["type"] = "UART";
				json["idx"] = idx;
				json["payload"] = QString(data);
				auto msg = QJsonDocument(json).toJson(QJsonDocument::Compact);
				fpga_instance->call_send_fpga_msg(msg);
				std::cout << "UART OUT" << std::endl;
			}
			return 0;
		};
};

#define HEXPLAY_ID 1004
class HEXPLAY : public Periph
{
	// idx -- /dev/serialX
	private:
	uint32_t number;
	uint32_t cooldown;
	uint32_t report;
	// pins[]: an{0,1,2}, d{0,1,2,3}
	public:
		HEXPLAY(int type, int idx, bool needpoll, int pincnt, int pins[]) :
			Periph(type, idx, needpoll, pincnt, pins) {
				number = 0;
				cooldown = 0;
				report = 0;
			}
		virtual int poll() override {
			uint32_t newnumber;
			int an = 0;
			int d = 0;
			for (int i = 0; i <= 2; i++)
				an = (an << 2) + gpioRead(pins[2-i]) ? 1 : 0;
			an = an % 8; // just in case... of nothing
			for (int i = 0; i <= 3; i++)
				d = (d << 2) + gpioRead(pins[3-i]) ? 1 : 0;
			d = d % 16;
			newnumber = (number & ~(0xf << (an*4))) | (d << (an*4));
			if (newnumber != number) {
				number = newnumber;
				report = 1;
			}
			if (cooldown) cooldown--;
			if (report && cooldown == 0) {
				cooldown = 100; // avoid flooding frontend
				report = 0;
				QJsonObject json;
				QString s = QString::number(number);
				json["type"] = "HEXPLAY";
				json["idx"] = idx;
				json["payload"] = s;
				auto msg = QJsonDocument(json).toJson(QJsonDocument::Compact);
				fpga_instance->call_send_fpga_msg(msg);
				std::cout << "HEXPLAY CHANGE" << std::endl;
			}
			return 0;
		};
};

#endif /* PERIPHERALS_H */
