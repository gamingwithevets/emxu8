#pragma once
#include "peripheral.h"

class Standby : public Peripheral {
public:
	uint8_t stpacp_last;
	bool stpacp_enabled;
	Standby();
	void Reset() override;
};
