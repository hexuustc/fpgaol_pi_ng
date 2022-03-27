/**
 * File              : peripherals.cpp
 * License           : GPL-3.0-or-later
 * Author            : FPGAOL
 * Date              : 2022.03.12
 * Last Modified Date: 2022.03.12
 */
#include "peripherals.h"
#include <iostream>

// storage of all peripherals -- periph ID as key
// vectors contains array of different kinds of peripherals
std::map<int, std::vector<Periph*> > periph_arr;

std::map<std::string, int> periphstr2id_map = {
	{"LED", LED_ID},
	{"BTN", BTN_ID},
	{"UART", UART_ID},
	{"HEXPLAY", HEXPLAY_ID}
};

std::map<int, std::string> periphid2str_map = {
	{LED_ID, "LED"},
	{BTN_ID, "BTN"},
	{UART_ID, "UART"},
	{HEXPLAY_ID, "HEXPLAY"}
};

// TODO: merge these map into one
// and don't want to use BOOST
std::map<int, int> pincnt_map = {
	{LED_ID, 1},
	{BTN_ID, 1},
	{UART_ID, 0}, // specially allocated
	{HEXPLAY_ID, 7}
};

// 0 -- no poll
// 1 -- slow poll
// 2 -- fast poll
// TODO: use this
std::map<int, int> needpoll_map = {
	{LED_ID, 1},
	{BTN_ID, 0},
	{UART_ID, 1},
	{HEXPLAY_ID, 2}
};

