#pragma once
#include "emxu8.h"
#include <iostream>
#include <cstdint>
#include <map>
#include <array>
#include <vector>

namespace emxu8 {
	class U8Core;

	class U8Interrupter {
		U8Core *core;

	protected:
		struct interrupt {
			uint16_t vector_addr;
			std::string name;
		};

		std::map<uint16_t, std::array<interrupt, 8>> interrupts{};

	public:
		explicit U8Interrupter(U8Core *core);
		void AllocateSFR(uint16_t start, uint16_t size);
		void AddInterrupt(uint16_t idx, uint8_t bit, uint16_t vector_addr, const std::string& name);
		void TryRaiseInterrupt(uint16_t idx, uint8_t bit);
		uint16_t GetIEVectorAddress(uint16_t addr, uint8_t bit);
		uint16_t GetIRQVectorAddress(uint16_t addr, uint8_t bit);

		uint16_t ie_start{};
		uint16_t irq_start{};
		bool mask_int = false;

		struct call {
			uint32_t func_addr;
			uint32_t ret_addr;
			uint16_t lr_loc;
			interrupt intr;
		};
		std::vector<call> callstack;

		friend class U8Core;
	};
}
