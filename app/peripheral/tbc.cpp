#include "tbc.h"
#include "clockgen.h"
#include "../mcu/mcu.h"

void ltbr_write(emxu8::U8Core *, uint16_t, uint8_t) {
	*MCU::Instance->tbc->ltbr = 0;
	MCU::Instance->tbc->ltbc_reset = true;
	MCU::Instance->clock_gen->lsclk_tick = true;
	MCU::Instance->clock_gen->lsclk_tick_ctr = 0;
	MCU::Instance->clock_gen->lsclk_time_ctr = 0;
	MCU::Instance->clock_gen->lsclk_freq_add = 0;
}

uint8_t ltbadj_read(emxu8::U8Core *, uint16_t addr) {
	return static_cast<uint8_t>((MCU::Instance->tbc->ltbadj & 0x7ff) >> (addr - 6) * 8);
}

void ltbadj_write(emxu8::U8Core *, uint16_t addr, uint8_t val) {
	int offset = addr - 6;
	MCU::Instance->tbc->ltbadj = MCU::Instance->tbc->ltbadj & ~(0xff << offset * 8) | val << offset * 8;
	MCU::Instance->tbc->ltbadj &= 0x7ff;
	if (MCU::Instance->tbc->ltbadj != 0) MCU::Instance->clock_gen->lsclk_thresh = MCU::Instance->clock_gen->lsclk_freq * (1 + 0x200000 / static_cast<short>(MCU::Instance->tbc->ltbadj)) / MCU::Instance->clock_gen->frequency;
	else MCU::Instance->clock_gen->lsclk_thresh = 0;
}

TimeBaseCounter::TimeBaseCounter() : U8Peripheral(&MCU::Instance->core) {
	core->RegisterSFR(0xc, 1, emxu8::U8Core::DefaultRead<0xff>, &ltbr_write);
	core->RegisterSFR(6, 2, &ltbadj_read, &ltbadj_write);
	ltbr = &core->sfrs[0xc];
	Reset();
}

unsigned int TimeBaseCounter::Tick() {
	if (ltbc_reset) {
		mcu->clock_gen->lsclk_out = 0xff;
		ltbr_reset_tick = true;
		core->interrupter->TryRaiseInterrupt(0, 6);
		core->interrupter->TryRaiseInterrupt(0, 7);
		core->interrupter->TryRaiseInterrupt(1, 0);
		core->interrupter->TryRaiseInterrupt(1, 1);
	}
	if (mcu->clock_gen->lsclk_tick) {
		ltbc_h = ltbc_h + 1 & 0x7f;
		mcu->clock_gen->lsclk_out_h = ltbc_h - 1 & ~ltbc_h;

		if (ltbr_reset_tick) {
			ltbr_reset_tick = false;
			return 0;
		}

		mcu->clock_gen->lsclk_out = 0;
		if (mcu->clock_gen->lsclk_out_h & 0x40) {
			(*ltbr)++;
			curr_out = mcu->clock_gen->lsclk_out = *ltbr - 1 & ~*ltbr;

			if (curr_out & 1) core->interrupter->TryRaiseInterrupt(0, 6);
			if (curr_out & 4) core->interrupter->TryRaiseInterrupt(0, 7);
			if (curr_out & 0x10) core->interrupter->TryRaiseInterrupt(1, 0);
			if (curr_out & 0x40) core->interrupter->TryRaiseInterrupt(1, 1);
		}
	}
	return 0;
}

void TimeBaseCounter::Reset() {
	*ltbr = 0;
	ltbc_h = 0;
	ltbadj = 0;
	ltbc_reset = false;

	curr_out = 0;
	ltbr_reset_tick = false;
}
