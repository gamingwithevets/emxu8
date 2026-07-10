#include "emxu8/decoder.h"
#include "emxu8/emxu8.h"

using namespace emxu8;

U8Decoder::U8Decoder(U8Core *core) : core(core) {}

void U8Decoder::Tick() {
	if (core->fetcher->fetches.size() > 1)
		cur_pc = std::prev(std::prev(core->fetcher->fetches.end()))->first;

	if (active && core->fetcher->fetches.size() > 1) {
		if (decodes.size() > 5) decodes.pop_front();

		uint16_t opcode = std::prev(std::prev(core->fetcher->fetches.end()))->second;
		decodes.emplace_back(cur_pc, DecodeOpcode(opcode));
		cur_pc += 2;
	}
}

U8Decoder::instruction U8Decoder::DecodeOpcode(uint16_t opcode) {
	instruction instr{};
	instr.opcode = opcode;

	for (const instruction_raw &ins : instructions)
		if ((opcode & ins.mask) == ins.opcode) {
			instr.operand = ins.operand;
			instr.arg1.argtype = ins.arg1.argtype;
			instr.arg1.flags = ins.arg1.flags;
			instr.arg1.value = (opcode & ins.arg1.mask) >> ins.arg1.shift;
			instr.arg2.argtype = ins.arg2.argtype;
			instr.arg2.flags = ins.arg2.flags;
			instr.arg2.value = (opcode & ins.arg2.mask) >> ins.arg2.shift;
			break;
		}

	return instr;
}

void U8Decoder::Reset() {
	decodes.clear();
	cur_pc = 0x100000;
	active = true;
}

std::pair<uint32_t, std::optional<U8Decoder::instruction>> U8Decoder::GetNextExec() {
	if (decodes.size() >= 2) {
		auto it = decodes.end();
		std::advance(it, -2);
		return *it;
	}
	return {0x100000, std::nullopt};
}
