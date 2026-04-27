#pragma once
#include <emxu8/emxu8.h>
#include "../mcu/mcu.h"
#include "clockgen.h"
#include "tbc.h"

class Peripheral : public emxu8::U8Peripheral {
protected:
	MCU *mcu = MCU::Instance;
public:
	bool enabled = true;
	enum ClockType
	{
		CLOCK_NONE,
		CLOCK_LSCLK,
		CLOCK_HSCLK
	};
	ClockType clock_type = CLOCK_NONE;
	explicit Peripheral() : U8Peripheral(&MCU::Instance->core) {}
	virtual unsigned int Tick2() { return 0; }
	virtual void ResetLSCLK() {}
	unsigned int Tick() override {
		if (!enabled) return 0;
		switch (clock_type) {
			case CLOCK_NONE: return Tick2();
			case CLOCK_LSCLK:
				if (mcu->tbc->ltbc_reset) ResetLSCLK();
				if (mcu->clock_gen->lsclk_tick) return Tick2();
				break;
			case CLOCK_HSCLK:
				if (mcu->clock_gen->hsclk_tick) Tick2();
				break;
			default: break;
		}
		return 0;
	}
};
