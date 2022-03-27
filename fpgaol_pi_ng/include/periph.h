/**
 * File              : periph.h
 * License           : GPL-3.0-or-later
 * Author            : FPGAOL
 * Date              : 2022.03.12
 * Last Modified Date: 2022.03.12
 */
#ifndef PERIPH_H
#define PERIPH_H

#include <QObject>
#include <QString>

#define MAX_GPIO 50
#define PERIPH_MAX_PINS 20

//class Periph : public QObject
class Periph
{
	private:
	public:
		int type;
		int idx;
		bool needpoll;
		int pins[PERIPH_MAX_PINS];
		Periph();
		Periph(int type, int idx, bool needpoll, int pincnt, int pins[]);
		~Periph();
	// TODO: how about a delayed task? need some mechanism to do it
	public:
		//Periph& operator=(const Periph& p);
		// when the peripheral requires attention
		// send a message (e.g. containing status change)
		// fpga_instance global variable is used, bad...
		// don't know if must in slot yet..
		int send_msg();
		// poll peripherals, currently no interrupt
		// should return fast
		// return: 0 nothing or already handled, none-zero need attention
		virtual int poll() { return 0; };
		// callback of the signal notify(e.g. write value in msg to hw)
		// nobody prevent putting a send_msg call in this -- so 
		// sending a notifiction back is OK
		virtual int on_notify(QString msg) { return 0; };

		// when frontend requires device attention, a signal is sent
		// this is automatically connect to on_notify
		//void notify(QString msg);
};

#endif /* PERIPH_H */
