#include <cmath>
#include "clockgen.h"
#include "../mcu/mcu.h"

void fcon_write(emxu8::U8Core *, uint16_t, uint8_t val) {
	uint8_t osclk = (val & 0x70) >> 4;
	*MCU::Instance->clock_gen->fcon = val & 0x73;
	MCU::Instance->clock_gen->clock_div = std::pow(2, osclk == 0 ? osclk : osclk - 1);
	MCU::Instance->clock_gen->lsclk_mode = *MCU::Instance->clock_gen->fcon == 1;
}

void htbr_write(emxu8::U8Core *, uint16_t, uint8_t) {
	*MCU::Instance->clock_gen->htbr = 0;
	MCU::Instance->clock_gen->hsclk_out = 0xff;
	MCU::Instance->clock_gen->htbc_reset = true;
	MCU::Instance->clock_gen->hsclk_tick = true;
	MCU::Instance->clock_gen->hsclk_tick_ctr = 0;
}

ClockGen::ClockGen(emxu8::U8Core *core, int _frequency) : U8Peripheral(core) {
	frequency = _frequency;

	core->RegisterSFR(0xa, 1, emxu8::U8Core::DefaultRead<0xff>, &fcon_write);
	core->RegisterSFR(0xd, 1, emxu8::U8Core::DefaultRead<0xff>, &htbr_write);
	fcon = &core->sfrs[0xa];
	htbr = &core->sfrs[0xd];

	lsclk_freq = frequency > 0 ? frequency / 32 : 16384;
	Reset();
}

unsigned int ClockGen::Tick() {
	if (!MCU::Instance->halt && ++hsclk_tick_ctr >= clock_div) {
		hsclk_tick = true;
		hsclk_tick_ctr = 0;
		if (htbc_reset) htbc_reset = false;
		else {
			hsclk_out = 0;
			if (++hsclk_time_ctr >= 128) {
				(*htbr)++;
				hsclk_out = *htbr - 1 & ~*htbr;
				hsclk_time_ctr = 0;
			}
		}
	}

	if (lsclk_mode) {
		long long current_period;
		if (frequency > 0) {
			current_period = (long long)frequency / lsclk_freq;
			// Ensure a minimum period of 1 to avoid division by zero or infinite speed if frequency is extremely high
			if (current_period == 0) current_period = 1;
		} else if (frequency < 0) {
			// Negative frequency means RAW speed! Tick every cycle!
			current_period = 1;
		} else { // frequency == 0
			// If frequency is 0, set a very large period to effectively stop the clock
			current_period = 0x7FFFFFFFFFFFFFFFLL; // Max long long value
		}

		if (++lsclk_tick_ctr >= current_period + lsclk_freq_add) {
			lsclk_tick = true;
			lsclk_tick_ctr = 0;
			if (lsclk_freq_add != 0) {
				lsclk_freq_add = 0;
				lsclk_time_ctr = 0;
			}
			if (lsclk_thresh > 0) {
				if (++lsclk_time_ctr >= lsclk_thresh) lsclk_freq_add = 1;
			} else if (lsclk_thresh < 0) {
				if (++lsclk_time_ctr >= -lsclk_thresh) lsclk_freq_add = -1;
			}
		}
	}
	return 0;
}

void ClockGen::Reset() {
	*fcon = 0;
	*htbr = 0;
	lsclk_out = 0;
	lsclk_out_h = 0;
	hsclk_out = 0;

	clock_div = 1;
	lsclk_mode = false;

	lsclk_tick = false;
	hsclk_tick = false;
	htbc_reset = false;

	lsclk_tick_ctr = 0;
	lsclk_time_ctr = 0;
	lsclk_freq_add = 0;
	lsclk_thresh = 0;
	hsclk_tick_ctr = 0;
	hsclk_time_ctr = 0;
}
