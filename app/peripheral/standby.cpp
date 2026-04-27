#include "standby.h"
#include "../mcu/mcu.h"

void stpacp_write(emxu8::U8Core *, uint16_t, uint8_t val) {
	if ((val & 0xf0) == 0xa0 && (MCU::Instance->standby->stpacp_last & 0xf0) == 0x50) MCU::Instance->standby->stpacp_enabled = true;
	MCU::Instance->standby->stpacp_last = val;
}

void sbycon_write(emxu8::U8Core *core, uint16_t, uint8_t val) {
	if (val & 1) {
		MCU::Instance->halt = true;
		core->active = false;
	} else if (val & 2 && MCU::Instance->standby->stpacp_enabled) {
		MCU::Instance->standby->stpacp_enabled = false;
		core->active = false;
	}
}

Standby::Standby() {
	core->RegisterSFR(8, 1, emxu8::U8Core::NoRead, &stpacp_write);
	core->RegisterSFR(9, 1, emxu8::U8Core::NoRead, &sbycon_write);
	Reset();
}

void Standby::Reset() {
	stpacp_last = 0;
	stpacp_enabled = false;
}

