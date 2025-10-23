#pragma once
#include "emxu8.h"
#include "compilerdefs.h"
#include <iostream>

namespace emxu8 {
	class EMXU8_API U8Disassembler {
		U8Core *core;

		static const std::string mnemonic[];
	public:
		explicit U8Disassembler(U8Core *core);
		std::string Disassemble(uint16_t pc, uint8_t csr, uint8_t &ins_len);
	};
}
