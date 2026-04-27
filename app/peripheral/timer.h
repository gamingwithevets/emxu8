#pragma once
#include "peripheral.h"

class Timer : public Peripheral {
	uint64_t ext_to_int_counter;
public:
	uint8_t *tm0con0;
	uint8_t *tm0con1;
	size_t timer_freq_div;

	explicit Timer();
	unsigned int Tick2() override;
	void Reset() override;
};
