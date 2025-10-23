#pragma once
#include "../peripheral/screen.h"
#include <emxu8/emxu8.h>

class MCU {
public:
	emxu8::U8Core core;
	uint8_t *ram;
	Screen screen;
	int frequency = -1;

	MCU(uint8_t *rom, uint16_t romwin_end, uint16_t ramstart, uint16_t ramsize);
	unsigned int Tick();
	void Reset();
	void InitializeRAM();
};
