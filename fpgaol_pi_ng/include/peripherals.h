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
		//int on_notify(QString msg) { return 0; }
		UART(int type, int idx, bool needpoll, int pincnt, int pins[], int baud) :
			Periph(type, idx, needpoll, pincnt, pins) {
				baudrate = baud;
				char serstr[20] = "/dev/serial0";
				serstr[11] += idx;
				serport = serOpen(serstr, baudrate, NULL);
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

#endif /* PERIPHERALS_H */
