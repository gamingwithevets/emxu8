#include "emxu8/executor.h"
#include "emxu8/decoder.h"
#include "emxu8/emxu8.h"
#include "emxu8/compilerdefs.h"

using namespace emxu8;

template<typename T>
constexpr static T from_buf(const uint8_t* data) {
	T value = 0;
	for (size_t i = 0; i < sizeof(T); i++) value |= static_cast<T>(data[i]) << (8 * i);
	return value;
}

template<typename T>
constexpr static void to_buf(T value, uint8_t *data) {
	for (size_t i = 0; i < sizeof(T); ++i) data[i] = static_cast<uint8_t>(value >> (8 * i)); // Little endian
}

U8Executor::U8Executor(U8Core *core) : core(core), next_pc(0) {}

uint8_t U8Executor::Add(uint8_t a, uint8_t b) {
	uint16_t wide_sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(b);
	uint8_t result = static_cast<uint8_t>(wide_sum & 0xff);

	core->psw.c = wide_sum > 0xff;
	SetZSFlags(result);
	core->psw.ov = ~(a ^ b) & (a ^ result) & 0x80;
	core->psw.hc = (a & 0xf) + (b & 0xf) > 0xf;

	return result;
}

uint16_t U8Executor::Add(uint16_t a, uint16_t b) {
	uint32_t wide_sum = static_cast<uint32_t>(a) + static_cast<uint32_t>(b);
	uint16_t result = static_cast<uint16_t>(wide_sum & 0xffff);

	core->psw.c = wide_sum > 0xffff;
	SetZSFlags(result);
	core->psw.ov = ~(a ^ b) & (a ^ result) & 0x8000;
	core->psw.hc = (a & 0xfff) + (b & 0xfff) > 0xfff;

	return result;
}

uint8_t U8Executor::AddCarry(uint8_t a, uint8_t b) {
	uint8_t c = core->psw.c;
	uint16_t wide_sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(b) + static_cast<uint16_t>(c);
	uint8_t result = static_cast<uint8_t>(wide_sum & 0xff);

	core->psw.c = wide_sum > 0xff;
	SetZSFlags(result, core->psw.z);
	core->psw.ov = ~(a ^ b) & (a ^ result) & 0x80;
	core->psw.hc = (a & 0xf) + (b & 0xf) + c > 0xf;

	return result;
}

uint8_t U8Executor::Subtract(uint8_t a, uint8_t b) {
	uint16_t wide_diff = static_cast<uint16_t>(a) - static_cast<uint16_t>(b);
	uint8_t result = static_cast<uint8_t>(wide_diff & 0xff);

	core->psw.c = a < b;
	SetZSFlags(result);
	core->psw.ov = (a ^ b) & (a ^ result) & 0x80;
	core->psw.hc = (a & 0xf) < (b & 0xf);

	return result;
}

uint16_t U8Executor::Subtract(uint16_t a, uint16_t b) {
	uint32_t wide_diff = static_cast<uint32_t>(a) - static_cast<uint32_t>(b);
	uint16_t result = static_cast<uint16_t>(wide_diff & 0xffff);

	core->psw.c = a < b;
	SetZSFlags(result);
	core->psw.ov = (a ^ b) & (a ^ result) & 0x8000;
	core->psw.hc = (a & 0xfff) < (b & 0xfff);

	return result;
}

uint8_t U8Executor::SubtractCarry(uint8_t a, uint8_t b) {
	uint8_t c = core->psw.c;
	uint16_t wide_diff = static_cast<uint16_t>(a) - static_cast<uint16_t>(b) - static_cast<uint16_t>(c);
	uint16_t result = static_cast<uint8_t>(wide_diff & 0xff);

	core->psw.c = static_cast<uint16_t>(a) < static_cast<uint16_t>(b) + static_cast<uint16_t>(c);
	SetZSFlags(result, core->psw.z);
	core->psw.ov = (a ^ b) & (a ^ result) & 0x80;
	core->psw.hc = (a & 0xf) < (b & 0xf) + c;

	return result;
}

void U8Executor::SetZSFlags(uint8_t result, bool oldz) {
	core->psw.z = oldz && result == 0;
	core->psw.s = result & 0x80;
}

void U8Executor::SetZSFlags(uint16_t result) {
	core->psw.z = result == 0;
	core->psw.s = result & 0x8000;
}

void U8Executor::SetZSFlags(uint32_t result) {
	core->psw.z = result == 0;
	core->psw.s = result & 0x80000000;
}

void U8Executor::SetZSFlags(uint64_t result) {
	core->psw.z = result == 0;
	core->psw.s = result & 0x8000000000000000;
}

#define WAIT_CYCLES(c) do { int __c = (c); for (int __i = 0; __i < __c; __i++) co_yield 1; } while (0)

InstrTask U8Executor::ExecuteLoop() {
	if (pause) {
		pause = false;
		WAIT_CYCLES(1);
		co_return;
	}

	if (core->decoder->decodes.size() < 2) {
		WAIT_CYCLES(1);
		co_return;
	}
	cur_pc = std::prev(std::prev(core->decoder->decodes.end()))->first;
	U8Decoder::instruction instr = std::prev(std::prev(core->decoder->decodes.end()))->second;
	current_task_cycle_count = 0;

	auto ReadDataMemory = [&](const uint16_t addr, const uint8_t segment, const uint16_t size, uint8_t *out) -> int {
		uint16_t _addr = addr;
		if (size > 1) _addr &= 0xfffe;
		return core->ReadDataMemory(_addr, segment, size, out);
	};
	auto WriteDataMemory = [&](const uint16_t addr, const uint8_t segment, const uint16_t size, const uint8_t *in) -> int {
		uint16_t _addr = addr;
		if (size > 1) _addr &= 0xfffe;
		return core->WriteDataMemory(_addr, segment, size, in);
	};

	uint16_t word = std::prev(std::prev(core->fetcher->fetches.end()))->second;

	next_pc = cur_pc + 2;
	if (
		instr.arg1.argtype == U8Decoder::ARG_ADR ||
		(instr.arg2.argtype == U8Decoder::ARG_ADR && instr.arg2.flags != 2) ||
		(instr.arg1.argtype == U8Decoder::ARG_PTR_REG && instr.arg1.flags) || (instr.arg2.argtype == U8Decoder::ARG_PTR_REG && instr.arg2.flags)
		) {
		next_pc += 2;
		pause = true;
	}
	next_pc &= 0x1ffffe;

	switch (instr.operand) {
		case U8Decoder::OP_ADD:
			if (instr.arg1.argtype == U8Decoder::ARG_REG) {
				if (instr.arg1.flags == 1) {
					uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value];
					uint8_t a = core->r[instr.arg1.value];
					core->r[instr.arg1.value] = Add(a, b);
				} else {
					uint16_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? sign_extend(instr.arg2.value, instr.arg2.flags) : core->GetER(instr.arg2.value);
					uint16_t a = core->GetER(instr.arg1.value);
					core->SetER(instr.arg1.value, Add(a, b));
				}
				current_task_cycle_count = instr.arg1.flags;
			} else {
				core->sp += static_cast<int8_t>(instr.arg2.value);
				current_task_cycle_count = 2;
			}
			break;
		case U8Decoder::OP_ADDC: {
			uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value];
			uint8_t a = core->r[instr.arg1.value];
			core->r[instr.arg1.value] = AddCarry(a, b);
			current_task_cycle_count = 1;
			break;
		}
		case U8Decoder::OP_AND: {
			uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value];
			SetZSFlags(core->r[instr.arg1.value] &= b);
			current_task_cycle_count = 1;
			break;
		}
		case U8Decoder::OP_CMP:
			if (instr.arg1.flags == 1) {
				uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value];
				uint8_t a = core->r[instr.arg1.value];
				Subtract(a, b);
			} else {
				uint16_t b = core->GetER(instr.arg2.value);
				uint16_t a = core->GetER(instr.arg1.value);
				Subtract(a, b);
			}
			current_task_cycle_count = instr.arg1.flags;
			break;
		case U8Decoder::OP_CMPC: {
			uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value];
			uint8_t a = core->r[instr.arg1.value];
			SubtractCarry(a, b);
			current_task_cycle_count = 1;
			break;
		}
		case U8Decoder::OP_MOV:
			switch (instr.arg1.argtype) {
				case U8Decoder::ARG_REG:
					if (instr.arg1.flags == 1) {
						switch (instr.arg2.argtype) {
							case U8Decoder::ARG_NUM:
							case U8Decoder::ARG_REG: {
								uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value];
								core->r[instr.arg1.value] = b;
								SetZSFlags(b);
								break;
							}
							case U8Decoder::ARG_CONREG:
								switch (instr.arg2.flags) {
								case 1: core->r[instr.arg1.value] = core->GetECSR(); break;
								case 3: core->r[instr.arg1.value] = core->GetEPSW(); break;
								case 4: core->r[instr.arg1.value] = core->psw.raw; break;
								}
								break;
							default: break;
						}
					} else {
						switch (instr.arg2.argtype) {
							case U8Decoder::ARG_NUM:
							case U8Decoder::ARG_REG: {
								uint16_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? sign_extend(instr.arg2.value, instr.arg2.flags) : core->GetER(instr.arg2.value);
								core->SetER(instr.arg1.value, b);
								SetZSFlags(b);
								break;
							}
							case U8Decoder::ARG_CONREG:
								switch (instr.arg2.flags) {
								case 0: core->SetER(instr.arg1.value, core->sp); break;
								case 2: core->SetER(instr.arg1.value, core->GetELR()); break;
								}
								break;
							default: break;
						}
					}
					current_task_cycle_count = instr.arg1.flags;
					break;
				case U8Decoder::ARG_PTR_EA: {
					uint8_t buf[8];
					switch (instr.arg2.flags) {
						case 1: to_buf(core->coprocessor->GetR(instr.arg2.value), buf); break;
						case 2: to_buf(core->coprocessor->GetER(instr.arg2.value), buf); break;
						case 4: to_buf(core->coprocessor->GetXR(instr.arg2.value), buf); break;
						case 8: to_buf(core->coprocessor->GetQR(instr.arg2.value), buf); break;
					}
					current_task_cycle_count += WriteDataMemory(core->ea, cur_dsr, instr.arg2.flags, buf);
					current_task_cycle_count += instr.arg2.flags;
					current_task_cycle_count += ea_inc;
					if (instr.arg1.flags) {
						core->ea += instr.arg2.flags;
						ea_inc = 2;
					}
					break;
				}
				case U8Decoder::ARG_CONREG:
					switch (instr.arg1.flags) {
						case 0:
							core->sp = core->GetER(instr.arg2.value);
							current_task_cycle_count = 1 + ea_inc;
							break;
						case 1:
							core->SetECSR(core->r[instr.arg2.value]);
							current_task_cycle_count = 2;
							break;
						case 2:
							core->SetELR(core->GetER(instr.arg2.value));
							current_task_cycle_count = 3;
							break;
						case 3:
							core->SetEPSW(core->r[instr.arg2.value]);
							current_task_cycle_count = 1;
							break;
						case 4:
							core->psw.raw = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value];
							current_task_cycle_count = 1;
							break;
					}
					break;
				case U8Decoder::ARG_REG_C: {
					uint8_t buf[8];
					current_task_cycle_count += ReadDataMemory(core->ea, cur_dsr, instr.arg1.flags, buf);
					current_task_cycle_count += instr.arg1.flags;
					current_task_cycle_count += ea_inc;
					if (instr.arg2.flags) {
						core->ea += instr.arg1.flags;
						ea_inc = 2;
					}
					switch (instr.arg1.flags) {
						case 1: core->coprocessor->SetR(instr.arg2.value, buf[0]); break;
						case 2: core->coprocessor->SetER(instr.arg2.value, from_buf<uint16_t>(buf)); break;
						case 4: core->coprocessor->SetXR(instr.arg2.value, from_buf<uint32_t>(buf)); break;
						case 8: core->coprocessor->SetQR(instr.arg2.value, from_buf<uint64_t>(buf)); break;
					}
					break;
				}
				default: break;
			}
			break;
		case U8Decoder::OP_OR: {
			uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value];
			SetZSFlags(core->r[instr.arg1.value] |= b);
			current_task_cycle_count = 1;
			break;
		}
		case U8Decoder::OP_XOR: {
			uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value];
			SetZSFlags(core->r[instr.arg1.value] ^= b);
			current_task_cycle_count = 1;
			break;
		}
		case U8Decoder::OP_SUB: {
			uint8_t b = core->r[instr.arg2.value];
			uint8_t a = core->r[instr.arg1.value];
			core->r[instr.arg1.value] = Subtract(a, b);
			current_task_cycle_count = 1;
			break;
		}
		case U8Decoder::OP_SUBC: {
			uint8_t b = core->r[instr.arg2.value];
			uint8_t a = core->r[instr.arg1.value];
			core->r[instr.arg1.value] = SubtractCarry(a, b);
			current_task_cycle_count = 1;
			break;
		}
		case U8Decoder::OP_SLL: {
			uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value] & 7;
			core->psw.c = core->r[instr.arg1.value] & 1 << (8 - b);
			core->r[instr.arg1.value] <<= b;
			current_task_cycle_count = 1 + ea_inc;
			break;
		}
		case U8Decoder::OP_SLLC: {
			uint8_t nm1 = instr.arg1.value - 1 & 15;
			uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value] & 7;
			uint16_t a = core->r[instr.arg1.value] << 8 | core->r[nm1];
			core->psw.c = core->r[instr.arg1.value] & 1 << (8 - b);
			a <<= b;
			core->r[instr.arg1.value] = a >> 8;
			current_task_cycle_count = 1 + ea_inc;
			break;
		}
		case U8Decoder::OP_SRA: {
			uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value] & 7;
			int8_t a = core->r[instr.arg1.value];
			core->psw.c = core->r[instr.arg1.value] & 1 << (b - 1);
			a >>= b;
			core->r[instr.arg1.value] = a;
			current_task_cycle_count = 1 + ea_inc;
			break;
		}
		case U8Decoder::OP_SRL: {
			uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value] & 7;
			core->psw.c = core->r[instr.arg1.value] & 1 << (b - 1);
			core->r[instr.arg1.value] >>= b;
			current_task_cycle_count = 1 + ea_inc;
			break;
		}
		case U8Decoder::OP_SRLC: {
			uint8_t np1 = instr.arg1.value + 1 & 15;
			uint8_t b = instr.arg2.argtype == U8Decoder::ARG_NUM ? instr.arg2.value : core->r[instr.arg2.value] & 7;
			uint16_t a = core->r[instr.arg1.value] | core->r[np1] << 8;
			core->psw.c = core->r[instr.arg1.value] & 1 << (b - 1);
			a >>= b;
			core->r[instr.arg1.value] = a >> 8;
			core->r[np1] = a;
			current_task_cycle_count = 1 + ea_inc;
			break;
		}
		case U8Decoder::OP_L:
			switch (instr.arg2.argtype) {
				case U8Decoder::ARG_PTR_EA: {
					uint8_t buf[8];
					current_task_cycle_count += ReadDataMemory(core->ea, cur_dsr, instr.arg1.flags, buf);
					if (instr.arg2.flags) {
						core->ea += instr.arg1.flags;
						ea_inc = 2;
					}
					switch (instr.arg1.flags) {
						case 1:
							core->r[instr.arg1.value] = buf[0];
							SetZSFlags(buf[0]);
							break;
						case 2:
							core->SetER(instr.arg1.value, from_buf<uint16_t>(buf));
							SetZSFlags(from_buf<uint16_t>(buf));
							break;
						case 4:
							core->SetXR(instr.arg1.value, from_buf<uint32_t>(buf));
							SetZSFlags(from_buf<uint32_t>(buf));
							break;
						case 8:
							core->SetQR(instr.arg1.value, from_buf<uint64_t>(buf));
							SetZSFlags(from_buf<uint64_t>(buf));
							break;
					}
					break;
				}
				case U8Decoder::ARG_PTR_REG: {
					uint8_t buf[8];
					uint16_t addr = core->GetER(instr.arg2.value);
					if (instr.arg2.flags) addr += word;
					current_task_cycle_count += ReadDataMemory(addr, cur_dsr, instr.arg1.flags, buf);
					current_task_cycle_count += ea_inc;
					switch (instr.arg1.flags) {
						case 1:
							core->r[instr.arg1.value] = buf[0];
							SetZSFlags(buf[0]);
							break;
						case 2:
							core->SetER(instr.arg1.value, from_buf<uint16_t>(buf));
							SetZSFlags(from_buf<uint16_t>(buf));
							break;
					}
					break;
				}
				case U8Decoder::ARG_PTR_BFP_DISP: {
					uint8_t buf[8];
					current_task_cycle_count += ReadDataMemory(core->GetER(instr.arg2.flags ? 12 : 14) + sign_extend(instr.arg2.value, 6), cur_dsr, instr.arg1.flags, buf);
					++current_task_cycle_count;
					current_task_cycle_count += ea_inc;
					switch (instr.arg1.flags) {
						case 1:
							core->r[instr.arg1.value] = buf[0];
							SetZSFlags(buf[0]);
							break;
						case 2:
							core->SetER(instr.arg1.value, from_buf<uint16_t>(buf));
							SetZSFlags(from_buf<uint16_t>(buf));
							break;
					}
					break;
				}
				case U8Decoder::ARG_ADR: {
					uint8_t buf[8];
					current_task_cycle_count += ReadDataMemory(word, cur_dsr, instr.arg1.flags, buf);
					++current_task_cycle_count;
					current_task_cycle_count += ea_inc;
					switch (instr.arg1.flags) {
						case 1:
							core->r[instr.arg1.value] = buf[0];
							SetZSFlags(buf[0]);
							break;
						case 2:
							core->SetER(instr.arg1.value, from_buf<uint16_t>(buf));
							SetZSFlags(from_buf<uint16_t>(buf));
							break;
					}
					break;
				}
				default: break;
			}
			current_task_cycle_count += instr.arg1.flags;
			break;
		case U8Decoder::OP_ST: {
			uint8_t buf[8];
			switch (instr.arg1.flags) {
				case 1:
					to_buf(core->r[instr.arg1.value], buf);
					break;
				case 2:
					to_buf(core->GetER(instr.arg1.value), buf);
					break;
				case 4:
					to_buf(core->GetXR(instr.arg1.value), buf);
					break;
				case 8:
					to_buf(core->GetQR(instr.arg1.value), buf);
					break;
			}
			switch (instr.arg2.argtype) {
				case U8Decoder::ARG_PTR_EA:
					current_task_cycle_count += WriteDataMemory(core->ea, cur_dsr, instr.arg1.flags, buf);
					if (instr.arg2.flags) {
						core->ea += instr.arg1.flags;
						ea_inc = 2;
					}
					break;
				case U8Decoder::ARG_PTR_REG: {
					uint16_t addr = core->GetER(instr.arg2.value);
					if (instr.arg2.flags) addr += word;
					current_task_cycle_count += WriteDataMemory(addr, cur_dsr, instr.arg1.flags, buf);
					current_task_cycle_count += ea_inc;
					break;
				}
				case U8Decoder::ARG_PTR_BFP_DISP:
					current_task_cycle_count += WriteDataMemory(core->GetER(instr.arg2.flags ? 12 : 14) + sign_extend(instr.arg2.value, 6), cur_dsr, instr.arg1.flags, buf);
					++current_task_cycle_count;
					current_task_cycle_count += ea_inc;
					break;
				case U8Decoder::ARG_ADR:
					current_task_cycle_count += WriteDataMemory(word, cur_dsr, instr.arg1.flags, buf);
					++current_task_cycle_count;
					current_task_cycle_count += ea_inc;
					break;
				default: break;
			}
			current_task_cycle_count += instr.arg1.flags;
			break;
		}
		case U8Decoder::OP_PUSH:
			switch (instr.arg1.argtype) {
				case U8Decoder::ARG_PUSH_LIST: {
					uint8_t buf[2];
					uint8_t sbuf[1];
					// ELR
					if (instr.arg1.value & 2) {
						if (core->memory_model == MM_LARGE) {
							sbuf[0] = core->GetECSR();
							core->sp -= 2;
							current_task_cycle_count += WriteDataMemory(core->sp, 0, 1, sbuf);
							current_task_cycle_count += 2;
						}
						to_buf(core->GetELR(), buf);
						core->sp -= 2;
						current_task_cycle_count += WriteDataMemory(core->sp, 0, 2, buf);
						current_task_cycle_count += 2;
					}
					// EPSW
					if (instr.arg1.value & 4) {
						sbuf[0] = core->GetEPSW();
						core->sp -= 2;
						current_task_cycle_count += WriteDataMemory(core->sp, 0, 1, sbuf);
						current_task_cycle_count += 2;
					}
					// LR
					if (instr.arg1.value & 8) {
						if (core->memory_model == MM_LARGE) {
							sbuf[0] = core->lcsr;
							core->sp -= 2;
							current_task_cycle_count += WriteDataMemory(core->sp, 0, 1, sbuf);
							current_task_cycle_count += 2;
						}
						to_buf(core->lr, buf);
						core->sp -= 2;
						current_task_cycle_count += WriteDataMemory(core->sp, 0, 2, buf);
						core->interrupter->callstack.back().lr_loc = core->sp;
						current_task_cycle_count += 2;
					}
					// EA
					if (instr.arg1.value & 1) {
						to_buf(core->ea, buf);
						core->sp -= 2;
						current_task_cycle_count += WriteDataMemory(core->sp, 0, 2, buf);
						current_task_cycle_count += 2;
					}
					break;
				}
				case U8Decoder::ARG_REG: {
					uint8_t buf[8];
					switch (instr.arg1.flags) {
						case 1: to_buf(core->r[instr.arg1.value], buf); break;
						case 2: to_buf(core->GetER(instr.arg1.value), buf); break;
						case 4: to_buf(core->GetXR(instr.arg1.value), buf); break;
						case 8: to_buf(core->GetQR(instr.arg1.value), buf); break;
					}
					int count = instr.arg1.flags == 1 ? 2 : instr.arg1.flags;
					core->sp -= count;
					current_task_cycle_count += WriteDataMemory(core->sp, 0, instr.arg1.flags, buf);
					current_task_cycle_count += count;
					break;
				}
				default: break;
			}
			current_task_cycle_count += ea_inc;
			break;
		case U8Decoder::OP_POP:
			switch (instr.arg1.argtype) {
				case U8Decoder::ARG_POP_LIST: {
					uint8_t buf[2];
					// EA
					if (instr.arg1.value & 1) {
						current_task_cycle_count += ReadDataMemory(core->sp, 0, 2, buf);
						core->ea = from_buf<uint16_t>(buf);
						core->sp += 2;
						current_task_cycle_count += 4;
					}
					// LR
					if (instr.arg1.value & 8) {
						current_task_cycle_count += ReadDataMemory(core->sp, 0, 2, buf);
						core->lr = from_buf<uint16_t>(buf);
						core->sp += 2;
						current_task_cycle_count += 2;
						if (core->memory_model == MM_LARGE) {
							current_task_cycle_count += ReadDataMemory(core->sp, 0, 2, buf);
							core->lcsr = from_buf<uint8_t>(buf);
							core->sp += 2;
							current_task_cycle_count += 2;
						}
					}
					// PSW
					if (instr.arg1.value & 4) {
						current_task_cycle_count += ReadDataMemory(core->sp, 0, 2, buf);
						core->psw.raw = from_buf<uint8_t>(buf);
						core->sp += 2;
						current_task_cycle_count += 2;
					}
					// PC
					if (instr.arg1.value & 2) {
						current_task_cycle_count += ReadDataMemory(core->sp, 0, 2, buf);
						core->pc = from_buf<uint16_t>(buf);
						core->sp += 2;
						current_task_cycle_count += 4;
						if (core->memory_model == MM_LARGE) {
							current_task_cycle_count += ReadDataMemory(core->sp, 0, 1, buf);
							core->csr = buf[0];
							core->sp += 2;
							++current_task_cycle_count;
						}
						next_pc = 0x100000;
						uint32_t tmp_cur_pc = cur_pc;
						core->ClearPipeline();
						if (!core->interrupter->callstack.empty()) {
							auto lr_loc = core->interrupter->callstack.back().lr_loc;
							uint8_t buf_loc[4];
							core->ReadDataMemory(lr_loc, 0, 4, buf_loc);
							if (lr_loc) {
								auto ret_addr_real = from_buf<uint32_t>(buf_loc) & 0xffffe;
								auto ret_addr = core->interrupter->callstack.back().ret_addr;
								if (ret_addr != ret_addr_real) printf("[U8Executor] WARNING: Popped stack entry return address does not match real stack (real %05X, stack %05X) @ %05X\n", ret_addr, ret_addr_real, tmp_cur_pc);
							}
							core->interrupter->callstack.pop_back();
							if (!lr_loc) core->interrupter->callstack.push_back({static_cast<uint32_t>(core->csr << 16 | core->pc), static_cast<uint32_t>(core->lcsr << 16 | core->lr)});
						}
					}
					break;
				}
				case U8Decoder::ARG_REG: {
					int count = instr.arg1.flags;
					uint8_t buf[8];
					current_task_cycle_count += ReadDataMemory(core->sp, 0, count, buf);
					count = count == 1 ? 2 : count;
					switch (instr.arg1.flags) {
						case 1: core->r[instr.arg1.value] = buf[0]; break;
						case 2: core->SetER(instr.arg1.value, from_buf<uint16_t>(buf)); break;
						case 4: core->SetXR(instr.arg1.value, from_buf<uint32_t>(buf)); break;
						case 8: core->SetQR(instr.arg1.value, from_buf<uint64_t>(buf)); break;
					}
					core->sp += count;
					current_task_cycle_count += count;
					break;
				}
				default: break;
			}
			current_task_cycle_count += ea_inc;
			break;
		case U8Decoder::OP_LEA:
			switch (instr.arg1.argtype) {
				case U8Decoder::ARG_PTR_REG:
					core->ea = core->GetER(instr.arg1.value);
					current_task_cycle_count = 1;
					if (instr.arg1.flags) {
						core->ea += word;
						++current_task_cycle_count;
					}
					break;
				case U8Decoder::ARG_ADR:
					core->ea = word;
					current_task_cycle_count = 2;
					break;
				default: break;
			}
			break;
		case U8Decoder::OP_DAA: {
			uint8_t adjustment = 0;
			bool c = core->psw.c;
			bool hc = false;
			uint8_t val = core->r[instr.arg1.value];

			if ((val & 0xf) > 9 || core->psw.hc) {
				adjustment |= 6;
				hc = true;
			}
			if (val >> 4 > 9 || c) {
				adjustment |= 0x60;
				c = true;
			}
			val += adjustment;
			core->r[instr.arg1.value] = val;
			core->psw.c = c;
			core->psw.hc = hc;
			SetZSFlags(val);
			current_task_cycle_count = 1;
			break;
		}
		case U8Decoder::OP_DAS: {
			uint8_t adjustment = 0;
			bool hc = false;
			uint8_t val = core->r[instr.arg1.value];

			if ((val & 0xf) > 9 || core->psw.hc) {
				adjustment |= 6;
				hc = true;
			}
			if (val >> 4 > 9 || core->psw.c) adjustment |= 0x60;
			int16_t result = static_cast<int16_t>(val) - adjustment;
			core->r[instr.arg1.value] = result;
			core->psw.c = result < 0;
			core->psw.hc = hc;
			SetZSFlags(static_cast<uint8_t>(result));
			current_task_cycle_count = 1;
			break;
		}
		case U8Decoder::OP_NEG: {
			uint8_t b = core->r[instr.arg1.value];
			core->r[instr.arg1.value] = Subtract(0, b);
			current_task_cycle_count = 1;
			break;
		}
		case U8Decoder::OP_SB:
			switch (instr.arg1.argtype) {
				case U8Decoder::ARG_ADR: {
					uint16_t addr = word;
					uint8_t val;
					current_task_cycle_count += ReadDataMemory(addr, cur_dsr, 1, &val);
					current_task_cycle_count += 2 + ea_inc;
					core->psw.z = !(val & 1 << instr.arg2.value);
					val |= 1 << instr.arg2.value;
					WriteDataMemory(addr, cur_dsr, 1, &val);
					break;
				}
				case U8Decoder::ARG_REG: {
					uint8_t val = core->r[instr.arg1.value];
					core->psw.z = !(val & 1 << instr.arg2.value);
					val |= 1 << instr.arg2.value;
					core->r[instr.arg1.value] = val;
					current_task_cycle_count = 1;
					break;
				}
				default: break;
			}
			break;
		case U8Decoder::OP_RB:
			switch (instr.arg1.argtype) {
				case U8Decoder::ARG_ADR: {
					uint16_t addr = word;
					uint8_t val;
					current_task_cycle_count += ReadDataMemory(addr, cur_dsr, 1, &val);
					current_task_cycle_count += 2 + ea_inc;
					core->psw.z = !(val & 1 << instr.arg2.value);
					val &= ~(1 << instr.arg2.value);
					WriteDataMemory(addr, cur_dsr, 1, &val);
					break;
				}
				case U8Decoder::ARG_REG: {
					uint8_t val = core->r[instr.arg1.value];
					core->psw.z = !(val & 1 << instr.arg2.value);
					val &= ~(1 << instr.arg2.value);
					core->r[instr.arg1.value] = val;
					current_task_cycle_count = 1;
					break;
				}
				default: break;
			}
			break;
		case U8Decoder::OP_TB: {
			uint8_t val;
			switch (instr.arg1.argtype) {
				case U8Decoder::ARG_ADR: {
					current_task_cycle_count += ReadDataMemory(word, cur_dsr, 1, &val);
					current_task_cycle_count += 2 + ea_inc;
					break;
				}
				case U8Decoder::ARG_REG:
					val = core->r[instr.arg1.value];
					current_task_cycle_count = 1;
					break;
				default: break;
			}
			core->psw.z = !(val & 1 << instr.arg2.value);
			break;
		}
		case U8Decoder::OP_EI:
			core->psw.mie = true;
			current_task_cycle_count = 1;
			break;
		case U8Decoder::OP_DI:
			core->psw.mie = false;
			current_task_cycle_count = 3;
			break;
		case U8Decoder::OP_SC:
			core->psw.c = true;
			current_task_cycle_count = 1;
			break;
		case U8Decoder::OP_RC:
			core->psw.c = false;
			current_task_cycle_count = 1;
			break;
		case U8Decoder::OP_CPLC:
			core->psw.c ^= 1;
			current_task_cycle_count = 1;
			break;
		case U8Decoder::OP_BC: {
			bool cond = false;
			switch (instr.arg1.value) {
				// GE/NC
				case 0:
					cond = !core->psw.c;
					break;
					// LT/CY
				case 1:
					cond = core->psw.c;
					break;
					// GT
				case 2:
					cond = !core->psw.c && !core->psw.z;
					break;
					// LE
				case 3:
					cond = core->psw.z || core->psw.c;
					break;
					// GES
				case 4:
					cond = !(core->psw.ov ^ core->psw.s);
					break;
					// LTS
				case 5:
					cond = core->psw.ov ^ core->psw.s;
					break;
					// GTS
				case 6:
					cond = !(core->psw.ov ^ core->psw.s | core->psw.z);
					break;
					// LES
				case 7:
					cond = core->psw.ov ^ core->psw.s | core->psw.z;
					break;
					// NE/NZ
				case 8:
					cond = !core->psw.z;
					break;
					// EQ/ZF (BZ)
				case 9:
					cond = core->psw.z;
					break;
					// NV
				case 10:
					cond = !core->psw.ov;
					break;
					// OV
				case 11:
					cond = core->psw.ov;
					break;
					// PS
				case 12:
					cond = !core->psw.s;
					break;
					// NS
				case 13:
					cond = core->psw.s;
					break;
					// AL
				case 14:
					cond = true;
					break;
			}
			current_task_cycle_count = 1;
			if (cond) {
				core->pc = next_pc + sign_extend(instr.arg2.value, 8) * 2;
				next_pc = 0x100000;
				current_task_cycle_count += 2;
				core->ClearPipeline();
			}
			current_task_cycle_count += ea_inc;
			break;
		}
		case U8Decoder::OP_EXTBW: {
			uint16_t val = sign_extend(core->r[instr.arg2.value], 16);
			core->r[instr.arg1.value] = static_cast<uint8_t>(val >> 8);
			SetZSFlags(core->r[instr.arg2.value]);
			current_task_cycle_count = 1;
			break;
		}
		case U8Decoder::OP_SWI:
			core->interrupter->TryRaiseInterrupt(instr.arg1.value, 8);
			current_task_cycle_count = 3 + ea_inc;
			break;
		case U8Decoder::OP_BRK:
			if (core->psw.elevel >= 2) core->RequestReset();
			else core->interrupter->TryRaiseInterrupt(0, 9);
			current_task_cycle_count = 7 + ea_inc;
			break;
		case U8Decoder::OP_BL:
			core->lr = next_pc;
			core->lcsr = core->csr;
		case U8Decoder::OP_B:
			switch (instr.arg1.argtype) {
				case U8Decoder::ARG_ADR:
					core->csr = instr.arg1.value;
					core->pc = word;
					break;
				case U8Decoder::ARG_REG:
					core->pc = core->GetER(instr.arg1.value);
					break;
				default: break;
			}
			next_pc = 0x100000;
			current_task_cycle_count = 2 + ea_inc;
			core->ClearPipeline();
			if (instr.operand == U8Decoder::OP_BL) core->interrupter->callstack.push_back({static_cast<uint32_t>(core->csr << 16 | core->pc), static_cast<uint32_t>(core->lcsr << 16 | core->lr)});
			break;
		case U8Decoder::OP_MUL: {
			uint16_t result = core->r[instr.arg1.value] * core->r[instr.arg2.value];
			core->psw.z = !result;
			core->SetER(instr.arg1.value, result);
			current_task_cycle_count = 9;
			break;
		}
		case U8Decoder::OP_DIV:
			if (core->r[instr.arg2.value] != 0) {
				uint16_t result = core->GetER(instr.arg1.value) / core->r[instr.arg2.value];
				uint8_t remainder = core->GetER(instr.arg1.value) % core->r[instr.arg2.value];
				core->psw.c = false;
				core->psw.z = !result;
				core->SetER(instr.arg1.value, result);
				core->r[instr.arg2.value] = remainder;
			} else core->psw.c = true;
			current_task_cycle_count = 17;
			break;
		case U8Decoder::OP_INC: {
			uint8_t val;
			current_task_cycle_count += ReadDataMemory(core->ea, cur_dsr, 1, &val);
			++val;
			current_task_cycle_count += WriteDataMemory(core->ea, cur_dsr, 1, &val);
			current_task_cycle_count += 2 + ea_inc;
			break;
		}
		case U8Decoder::OP_DEC: {
			uint8_t val;
			current_task_cycle_count += ReadDataMemory(core->ea, cur_dsr, 1, &val);
			--val;
			current_task_cycle_count += WriteDataMemory(core->ea, cur_dsr, 1, &val);
			current_task_cycle_count += 2 + ea_inc;
			break;
		}
		case U8Decoder::OP_RT:
			core->csr = core->lcsr;
			core->pc = core->lr;
			next_pc = 0x100000;
			core->ClearPipeline();
			current_task_cycle_count = 2 + ea_inc;
			if (!core->interrupter->callstack.empty()) core->interrupter->callstack.pop_back();
			break;
		case U8Decoder::OP_RTI:
			core->csr = core->GetECSR();
			core->pc = core->GetELR();
			core->psw.raw = core->GetEPSW();
			next_pc = 0x100000;
			core->ClearPipeline();
			current_task_cycle_count = 2 + ea_inc;
			if (!core->interrupter->callstack.empty()) core->interrupter->callstack.pop_back();
			break;
		case U8Decoder::OP_NOP:
			current_task_cycle_count = 1;
			break;
		case U8Decoder::OP_DSR:
			switch (instr.arg1.argtype) {
				case U8Decoder::ARG_BIT_OFFSET:
					core->dsr = instr.arg1.value;
					break;
				case U8Decoder::ARG_REG:
					core->dsr = core->r[instr.arg1.value];
					break;
				case U8Decoder::ARG_NONE:
					next_cdsr = 2;
					break;
				default: break;
			}
			next_dsr = 2;
			cur_dsr = core->dsr;
			current_task_cycle_count = 1;
			break;
		default:
			printf("[U8Executor] WARNING: Unknown instruction opcode %04X @ %05X\n", instr.opcode, cur_pc);
			current_task_cycle_count = 1;
			break;
	}
	if (next_dsr > 0) {
		--next_dsr;
		--next_cdsr;
		core->interrupter->mask_int = true;
	} else {
		cur_dsr = 0;
		core->interrupter->mask_int = false;
	}
	if (ea_inc > 0) --ea_inc;

	cur_pc = next_pc >= 0x100000 ? 0x100000 : next_pc;

	WAIT_CYCLES(current_task_cycle_count);
}

unsigned int U8Executor::Tick() {
	if (!task_running) {
		current_task = ExecuteLoop();
		task_running = true;
	}
	current_task.handle.resume();
	if (current_task.handle.done()) {
		current_task.handle.destroy();
		task_running = false;
	}
	return current_task_cycle_count;
}

void U8Executor::Reset() {
	if (task_running) {
		current_task.handle.destroy();
		task_running = false;
	}
	cur_pc = 0x100000;
	next_pc = 0x100000;
}
