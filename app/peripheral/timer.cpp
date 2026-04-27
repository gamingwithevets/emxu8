#include "timer.h"
#include "../mcu/mcu.h"
#include <cmath>

void tm0con0_write(emxu8::U8Core *, uint16_t, uint8_t val) {
	*MCU::Instance->timer->tm0con0 = val & 0xf;
	MCU::Instance->timer->timer_freq_div = std::pow(2, val & 7);
	MCU::Instance->timer->clock_type = val & 8 ? Peripheral::CLOCK_HSCLK : Peripheral::CLOCK_LSCLK;
}

Timer::Timer() {
	timer_freq_div = 1;
	clock_type = CLOCK_LSCLK;
	core->RegisterSFR(0x20, 2, emxu8::U8Core::DefaultRead<0xff>, emxu8::U8Core::DefaultWrite<0xff>);
	core->RegisterSFR(0x22, 2, emxu8::U8Core::DefaultRead<0xff>, [](emxu8::U8Core *core, uint16_t, uint8_t) {
		core->sfrs[0x22] = core->sfrs[0x23] = 0;
	});
	core->RegisterSFR(0x24, 1, emxu8::U8Core::DefaultRead<0x0f>, tm0con0_write);
	core->RegisterSFR(0x25, 1, emxu8::U8Core::DefaultRead<1>, emxu8::U8Core::DefaultWrite<1>);

	tm0con0 = &core->sfrs[0x24];
	tm0con1 = &core->sfrs[0x25];
}

unsigned int Timer::Tick2() {
	if (*tm0con1 && ++ext_to_int_counter >= timer_freq_div) {
		ext_to_int_counter = 0;
		uint16_t interval = core->sfrs[0x20] | core->sfrs[0x21] << 8;
		uint16_t counter = core->sfrs[0x22] | core->sfrs[0x23] << 8;
		if (!interval) {
			core->sfrs[0x22] = core->sfrs[0x23] = 0;
		} else {
			++counter;
			core->sfrs[0x22] = counter;
			core->sfrs[0x23] = counter >> 8;
			if (counter >= interval) {
				core->sfrs[0x22] = core->sfrs[0x23] = 0;
				core->interrupter->TryRaiseInterrupt(0, 5);
			}
		}
	}
	return 0;
}

void Timer::Reset() {
	ext_to_int_counter = 0;
	core->sfrs[0x20] = 0;
	core->sfrs[0x21] = 0;
	core->sfrs[0x22] = 0;
	core->sfrs[0x23] = 0;
	*tm0con0 = 0;
	*tm0con1 = 0;
}
