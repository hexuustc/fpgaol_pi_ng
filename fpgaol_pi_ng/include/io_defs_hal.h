#ifndef IO_DEFS_HAL
#define IO_DEFS_HAL

#include "periph.h"

// the hardware abstraction layer
// for unified interface on different hardwares... if we can
// TODO


// the 

// this "Gpio" is generic I/O for low-speed peripherals that
// don't pick on which specific I/O to use
// 
// Otherwise things get quite complicated -- just look at the
// ZYNQ customization interface
class Gpio
{
	private:
		int pin; // raspberry pi pin
		std::string fpin; // fpga pin(string like "AA28")
		bool used; // used or not?
		Periph* periph; // who used this?
	public:
		Gpio(int i_pin, std::string i_fpin) {
			pin = i_pin;
			fpin = i_fpin;
			used = false;
			periph = NULL;
		}
		~Gpio() {  }
		void Occupy(Periph* p) {
			used = true;
			periph = p;
		}
		Periph* Owner() {
			return periph;
		}
		// so far no process will "Un-occupy" gpios -- 
		// everything just got deleted after connection ended
};

#define IO_MAX 46
extern std::vector<Gpio> gpio_arr;

extern int BOARD_TYPE;
extern int GPIOLIB_TYPE;

void platform_dependent_gpio_init();
void platform_dependent_gpio_fini();

#endif
