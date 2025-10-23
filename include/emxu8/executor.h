#pragma once

#include "emxu8.h"

namespace emxu8 {
	class U8Core;

	class U8Executor {
		U8Core *core;
	public:
		explicit U8Executor(U8Core *core);
		unsigned int Tick();
		void Reset();

		bool active = true;
		bool pause = false;
		uint32_t cur_pc = 0x100000;
		uint32_t next_pc;
		int8_t next_dsr = 0;
	private:
		uint8_t Add(uint8_t a, uint8_t b);
		uint16_t Add(uint16_t a, uint16_t b);
		uint8_t AddCarry(uint8_t a, uint8_t b);
		uint8_t Subtract(uint8_t a, uint8_t b);
		uint16_t Subtract(uint16_t a, uint16_t b);
		uint8_t SubtractCarry(uint8_t a, uint8_t b);
		void SetZSFlags(uint8_t result, bool oldz = true);
		void SetZSFlags(uint16_t result);
		void SetZSFlags(uint32_t result);
		void SetZSFlags(uint64_t result);
	protected:
		int8_t ea_inc = 0;
		int8_t next_cdsr = 0;
		uint8_t cur_dsr = 0;

		friend class U8Interrupter;
	};
}
