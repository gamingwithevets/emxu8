#include "mcu.h"
#include "../peripheral/screen.h"
#include <cstdlib>

MCU::MCU(uint8_t *rom, uint16_t romwin_end, uint16_t ramstart, uint16_t ramsize) : core(romwin_end), screen(&core) {
	core.AddCodeRegion({0, 0, 0x10000, rom});
	core.AddCodeRegion({1, 0, 0x10000, &rom[0x10000]});

	core.AddDataRegion({0, 0, romwin_end, true, false, rom});
	ram = static_cast<uint8_t *>(malloc(ramsize));
	InitializeRAM();
	core.AddDataRegion({0, ramstart, ramsize, true, true, ram});
	core.AddDataRegion({1, 0, 0x10000, true, false, &rom[0x10000]});
	core.AddDataRegion({8, 0, 0x10000, true, false, rom});

	core.SetMemoryModel(emxu8::MM_LARGE);

	frequency = 128 * 1024 * 1024;

	Reset();
}

unsigned int MCU::Tick() {
	return core.Tick();
}


void MCU::Reset() {
	core.Reset();
}

void MCU::InitializeRAM() {
	srand(time(NULL));
	for (size_t i = 0; i < sizeof(ram); i++) ram[i] = rand();
}

