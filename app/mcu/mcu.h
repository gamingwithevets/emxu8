#pragma once
#include "../peripheral/screen.h"
#include "../config/config.h"
#include <emxu8/emxu8.h>

class MCU {
public:
	static MCU *Instance;
	Config *config;

	emxu8::U8Core core;
	uint8_t *ram;
	uint16_t ramstart;
	uint16_t ramsize;
	Screen *screen;

	class ClockGen *clock_gen;
	class TimeBaseCounter *tbc;

	class Standby *standby;
	class Timer *timer;

	class Keyboard *kb;

	bool halt = false;

	MCU(Config *, uint8_t *, uint16_t, uint16_t, uint16_t);
	~MCU();
	unsigned int Tick();
	void Reset();
	void InitializeRAM();
};
