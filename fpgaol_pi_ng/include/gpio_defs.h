#ifndef GPIO_DEFS
#define GPIO_DEFS

#include<stdint.h>

const int LED[] = {0, 1, 2, 3, 4, 5, 6, 7};
const int SW[] = {8, 9, 10, 11, 12, 13, 14, 15};
const int SEG[] = {16, 17, 18, 19, 20, 21, 22};
const int BUTTON[] = {23};
const uint32_t LED_MASK = 
	(1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | 
	(1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);

const uint32_t SW_MASK = 
	(1 << 8) | (1 << 9) | (1 << 10) | (1 << 11) | 
	(1 << 12) | (1 << 13) | (1 << 14) | (1 << 15);

const uint32_t SEG_MASK = 
	(1 << 16) | (1 << 17) | (1 << 18) | (1 << 19) | 
	(1 << 20) | (1 << 21) | (1 << 22);

const uint32_t BUTTON_MASK = (1 << 23);

const uint32_t GPIO_MASK = LED_MASK | SW_MASK | SEG_MASK | BUTTON_MASK;

#endif
