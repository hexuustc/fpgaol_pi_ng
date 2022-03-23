#include "periph.h"
#include<iostream>

Periph::Periph()
{
	type = -1;
	idx = -1;
}

Periph::Periph(int i_type, int i_idx, bool i_needpoll, int i_pincnt, int i_pins[])
{
	type = i_type;
	idx = i_idx;
	needpoll = i_needpoll;
	for (int i = 0; i < i_pincnt; i++)
		pins[i] = i_pins[i];
	//QObject::connect(this, &Periph::notify, this, &Periph::on_notify);
}

//Periph& Periph::operator=(const Periph& p)
//{
	
//}

Periph::~Periph()
{
}

int Periph::send_msg()
{
}

//int Periph::on_notify(QString msg)
//{
	//return 0;
//}
