#pragma once
#include <emxu8/emxu8.h>

class ClockGen : public emxu8::U8Peripheral {
public:
	uint8_t *fcon;
	uint8_t *htbr;
	int lsclk_freq;
	long long lsclk_tick_ctr, hsclk_tick_ctr, hsclk_time_ctr, lsclk_time_ctr, lsclk_thresh;
	int lsclk_freq_add;

	uint8_t lsclk_out;
	uint8_t lsclk_out_h;
	uint8_t hsclk_out;

	int clock_div;
	bool lsclk_mode, lsclk_tick, hsclk_tick, htbc_reset;
	int frequency;

	explicit ClockGen(emxu8::U8Core *core, int _frequency = -1);
	void Reset() override;
	unsigned int Tick() override;
};
