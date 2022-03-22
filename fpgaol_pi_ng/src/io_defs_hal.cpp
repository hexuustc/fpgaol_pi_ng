#include "io_defs_hal.h"
#include "peripherals.h"

std::vector<Gpio> gpio_arr;

static std::string pi2_io2fpin[100] = {
"K17", "K18", "L14", "M14", "L18", "M18", "R12", "R13", "M13", "R18",
"T18", "N14", "P14", "P18", "M16", "M17", "T10", "T9", "U13", "T13",
"V14", "U14", "V11", "V12", "U12", "U11", "T11", "V17", "U16", "U18",
"U17", "V16", "J15", "K15", "F3", "F4", "G6", "E7", "D8" };

void platform_dependent_gpio_init()
{
	for (auto const &itr : periphstr2id_map) {
		std::vector<Periph> v;
		std::pair<int, std::vector<Periph> >p;
		p.first = itr.second;
		p.second = v;
		periph_arr.insert(p);
	}
	// FPGAOL 2
	// TODO: abstract this away
	for (int i = 0; i <= 45; i++) {
		gpio_arr.push_back(Gpio(i, pi2_io2fpin[i]));
	}
}

void platform_dependent_gpio_fini()
{
	periph_arr.clear();
	// FPGAOL 2
	gpio_arr.clear();
}
