#pragma once
#include <emxu8/emxu8.h>

class ClockGen : public emxu8::U8Peripheral {
public:
	explicit ClockGen(emxu8::U8Core *core);
	void Reset() override;
};
