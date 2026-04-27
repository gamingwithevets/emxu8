#pragma once
#include <emxu8/emxu8.h>
#include "../mcu/mcu.h"

class TimeBaseCounter : public emxu8::U8Peripheral {
protected:
	MCU *mcu = MCU::Instance;
public:
	uint8_t ltbadj;
	uint8_t *ltbr;

	uint8_t curr_out;
	bool ltbr_reset_tick;
	uint8_t ltbc_h;
	bool ltbc_reset;
	explicit TimeBaseCounter();
	void Reset() override;
	unsigned int Tick() override;
};
