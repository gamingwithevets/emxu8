#include "mcu.h"
#include "../peripheral/clockgen.h"
#include "../peripheral/tbc.h"
#include "../peripheral/standby.h"
#include "../peripheral/timer.h"
#include "../peripheral/keyboard.h"
#include <cstdlib>

MCU *MCU::Instance = nullptr;

MCU::MCU(Config *_config, uint8_t *rom, uint16_t romwin_end, uint16_t ramstart, uint16_t ramsize) : core(romwin_end), ramstart(ramstart), ramsize(ramsize) {
	Instance = this;
	config = _config;

	core.AddCodeRegion({0, 0, 0x10000, rom});
	core.AddCodeRegion({1, 0, 0x10000, &rom[0x10000]});

	core.AddDataRegion({0, 0, romwin_end, true, false, rom});
	ram = new uint8_t[ramsize];
	InitializeRAM();
	core.AddDataRegion({0, ramstart, ramsize, true, true, ram});
	core.AddDataRegion({1, 0, 0x10000, true, false, &rom[0x10000]});
	core.AddDataRegion({8, 0, 0x10000, true, false, rom});

	core.SetMemoryModel(emxu8::MM_LARGE);
	core.interrupter->AllocateSFR(0x10, 4);
	core.interrupter->AddInterrupt(0, 0, 8, "WDTINT");
	core.interrupter->AddInterrupt(0, 1, 0xa, "XI0INT");
	core.interrupter->AddInterrupt(0, 2, 0xc, "XI1INT");
	core.interrupter->AddInterrupt(0, 3, 0xe, "XI2INT");
	core.interrupter->AddInterrupt(0, 4, 0x10, "XI3INT");
	core.interrupter->AddInterrupt(0, 5, 0x12, "TM0INT");
	core.interrupter->AddInterrupt(0, 6, 0x14, "L256SINT");
	core.interrupter->AddInterrupt(0, 7, 0x16, "L1024SINT");
	core.interrupter->AddInterrupt(1, 0, 0x18, "L4096SINT");
	core.interrupter->AddInterrupt(1, 1, 0x1a, "L16384SINT");
	core.interrupter->AddInterrupt(1, 2, 0x1c, "SIO0INT");
	core.interrupter->AddInterrupt(1, 3, 0x1e, "I2C0INT");
	core.interrupter->AddInterrupt(1, 4, 0x20, "I2C1INT");

	core.RegisterSFR(0x18, 1, emxu8::U8Core::DefaultRead<0xff>, emxu8::U8Core::DefaultWrite<0xff>);

	screen = new Screen(&core, config);

	clock_gen = new ClockGen(&core, 512 * 1024);
	tbc = new TimeBaseCounter();

	standby = new Standby();
	timer = new Timer();

	kb = new Keyboard();

	Reset();
}

MCU::~MCU() {
	delete kb;
	delete timer;
	delete standby;
	delete tbc;
	delete clock_gen;
	delete screen;
	delete[] ram;
}

unsigned int MCU::Tick() {
	core.csr &= 1;
	unsigned int cycles = core.Tick(&clock_gen->hsclk_tick);
	clock_gen->lsclk_tick = false;
	tbc->ltbc_reset = false;
	clock_gen->hsclk_tick = false;
	return cycles;
}

void MCU::Reset() {
	halt = false;
	core.Reset();
}

void MCU::InitializeRAM() {
	srand(time(NULL));
	for (size_t i = 0; i < ramsize; i++) ram[i] = rand();
}

