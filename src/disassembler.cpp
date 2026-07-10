#include "emxu8/disassembler.h"
#include "emxu8/compilerdefs.h"
#include <format>
#include <string>

using namespace emxu8;

const std::string U8Disassembler::mnemonic[] = {
	"",

	// Arithmetic Instructions
	"ADD",
	"ADDC",
	"AND",
	"CMP",
	"CMPC",
	"MOV",
	"OR",
	"XOR",
	"SUB",
	"SUBC",

	// Shift Instructions
	"SLL",
	"SLLC",
	"SRA",
	"SRL",
	"SRLC",

	// Load/Store Instructions
	"L",
	"ST",

	// Control Register Access Instructions (ADD, MOV already included)

	// PUSH/POP Instructions
	"PUSH",
	"POP",

	// Coprocessor Data Transfer Instructions (mov already included)

	// EA Register Data Transfer Instructions
	"LEA",

	// ALU Instructions
	"DAA",
	"DAS",
	"NEG",

	// Bit Access Instructions
	"SB",
	"RB",
	"TB",

	// PSW Access Instructions
	"EI",
	"DI",
	"SC",
	"RC",
	"CPLC",

	// Conditional Relative Branch Instructions
	"BC",

	// Sign Extension Instruction
	"EXTBW",

	// Software Interrupt Instructions
	"SWI",
	"BRK",

	// Branch Instructions
	"B",
	"BL",

	// Multiplication and Division Instructions
	"MUL",
	"DIV",

	// Miscellaneous
	"INC [EA]",
	"DEC [EA]",
	"RT",
	"RTI",
	"NOP",
};

// ChatGPT
inline std::string parse_arg_num(unsigned int value, unsigned int bits, bool as_unsigned = false) {
	// Properly sign-extend into a signed int
	int signed_val;
	{
		unsigned int mask = 1u << (bits - 1);
		unsigned int extended = (value & ((1u << bits) - 1)); // clamp to bit-width
		signed_val = (extended ^ mask) - mask;
	}

	unsigned int abs_val;
	if (as_unsigned) {
		abs_val = value & ((1u << bits) - 1); // force into width
	} else {
		abs_val = signed_val < 0 ? static_cast<unsigned int>(-static_cast<long long>(signed_val))
								 : static_cast<unsigned int>(signed_val);
	}

	// Build hex string
	std::string hex;
	do {
		int digit = abs_val % 16;
		hex.insert(hex.begin(), digit < 10 ? '0' + digit : 'A' + digit - 10);
		abs_val /= 16;
	} while (abs_val > 0);

	// Prepend negative sign if needed
	if (!as_unsigned && signed_val < 0) hex = "-" + hex;

	// Prepend 0 if first digit is a letter
	if (hex[0] >= 'A' || (!as_unsigned && hex[0] == '-' && hex[1] >= 'A'))
		hex.insert((!as_unsigned && hex[0] == '-') ? 1 : 0, "0");

	hex += 'H'; // append the H suffix
	return hex;
}

static constexpr std::string tostring(unsigned int val) {
	return std::format("{}", val);
}

U8Disassembler::U8Disassembler(U8Core *core) : core(core) {}

std::string U8Disassembler::Disassemble(uint16_t pc, uint8_t csr, uint8_t &ins_len) {
	csr &= 0xf;
	uint16_t opcode[2];
	U8Decoder::instruction instr{};
	std::string out;
	std::string dsr_prefix;
	bool used_prefix = false;
	ins_len = 2;

	auto ParseArg = [&] (U8Decoder::instr_arg &arg) -> std::string {
		switch (arg.argtype) {
			case U8Decoder::ARG_REG:
				switch (arg.flags) {
				case 1: return "R" + tostring(arg.value);
				case 2: return "ER" + tostring(arg.value);
				case 4: return "XR" + tostring(arg.value);
				case 8: return "QR" + tostring(arg.value);
				}
				break;
			case U8Decoder::ARG_NUM: return "#" + parse_arg_num(arg.value, arg.flags);
			case U8Decoder::ARG_PTR_EA:
				if (!dsr_prefix.empty()) {
					used_prefix = true;
					dsr_prefix += ":";
				}
				return dsr_prefix + "[EA" + (arg.flags ? "+]" : "]");
			case U8Decoder::ARG_PTR_REG:
				if (!dsr_prefix.empty()) {
					used_prefix = true;
					dsr_prefix += ":";
				}
				if (arg.flags)  ins_len += 2;
				return dsr_prefix + (arg.flags ? parse_arg_num(opcode[1], 16) : "") + "[ER" + tostring(arg.value) + "]";
			case U8Decoder::ARG_PTR_BFP_DISP:
				if (!dsr_prefix.empty()) {
					used_prefix = true;
					dsr_prefix += ":";
				}
				return dsr_prefix + parse_arg_num(arg.value, 6) + "[" + (arg.flags ? "B" : "F") + "P]";
			case U8Decoder::ARG_ADR:
				switch (arg.flags) {
				case 0:
					ins_len += 2;
					return tostring(arg.value) + ":" + parse_arg_num(opcode[1], 16, true);
				case 1:
					if (!dsr_prefix.empty()) {
						used_prefix = true;
						dsr_prefix += ":";
					}
					ins_len += 2;
					return dsr_prefix + parse_arg_num(opcode[1], 16, true);
				case 2: return tostring(csr) + ":" + parse_arg_num(pc + 2 + sign_extend(arg.value, 8) * 2 & 0xfffe, 16, true);
				}
				break;
			case U8Decoder::ARG_CONREG:
				switch (arg.flags) {
				case 0: return "SP";
				case 1: return "ECSR";
				case 2: return "ELR";
				case 3: return "EPSW";
				case 4: return "PSW";
				}
				break;
			case U8Decoder::ARG_PUSH_LIST: {
				std::string regs;
				if (arg.value & 2) regs += "ELR";
				if (arg.value & 4) {
					regs += regs.empty() ? "" : ", ";
					regs += "EPSW";
				}
				if (arg.value & 8) {
					regs += regs.empty() ? "" : ", ";
					regs += "LR";
				}
				if (arg.value & 1) {
					regs += regs.empty() ? "" : ", ";
					regs += "EA";
				}
				return regs;
			}
			case U8Decoder::ARG_POP_LIST: {
				std::string regs;
				if (arg.value & 1) regs += "EA";
				if (arg.value & 8) {
					regs += regs.empty() ? "" : ", ";
					regs += "LR";
				}
				if (arg.value & 4) {
					regs += regs.empty() ? "" : ", ";
					regs += "PSW";
				}
				if (arg.value & 2) {
					regs += regs.empty() ? "" : ", ";
					regs += "PC";
				}
				return regs;
			}
			case U8Decoder::ARG_REG_C:
				switch (arg.flags) {
				case 1: return "CR" + tostring(arg.value);
				case 2: return "CER" + tostring(arg.value);
				case 4: return "CXR" + tostring(arg.value);
				case 8: return "CQR" + tostring(arg.value);
				}
				break;
			case U8Decoder::ARG_BIT_OFFSET: return tostring(arg.value);
			case U8Decoder::ARG_COND:
				switch (arg.value) {
					case 0: return "GE";
					case 1: return "LT";
					case 2: return "GT";
					case 3: return "LE";
					case 4: return "GES";
					case 5: return "LTS";
					case 6: return "GTS";
					case 7: return "LES";
					case 8: return "NE";
					case 9: return "EQ";
					case 0xa: return "NV";
					case 0xb: return "OV";
					case 0xc: return "PS";
					case 0xd: return "NS";
					case 0xe: return "AL";
				}
				break;
		}
		return "";
	};

	while (true) {
		opcode[0] = core->ReadCodeMemory(pc, csr);
		opcode[1] = core->ReadCodeMemory(pc+2, csr);

		for (const U8Decoder::instruction_raw &ins : U8Decoder::instructions)
			if ((opcode[0] & ins.mask) == ins.opcode) {
				instr.operand = ins.operand;
				instr.arg1.argtype = ins.arg1.argtype;
				instr.arg1.flags = ins.arg1.flags;
				instr.arg1.value = (opcode[0] & ins.arg1.mask) >> ins.arg1.shift;
				instr.arg2.argtype = ins.arg2.argtype;
				instr.arg2.flags = ins.arg2.flags;
				instr.arg2.value = (opcode[0] & ins.arg2.mask) >> ins.arg2.shift;
				break;
			}

		if (instr.operand == U8Decoder::OP_DSR) {
			dsr_prefix = ParseArg(instr.arg1);
			if (dsr_prefix.empty()) dsr_prefix = "DSR";
			ins_len += 2;
			pc += 2;
			continue;
		}
		if (
			instr.operand == U8Decoder::OP_NONE
			|| (instr.arg1.argtype == U8Decoder::ARG_COND && instr.arg1.value == 0xf)
			) {
			out = "DW ";
			if (opcode[0] > 0x9fff) out += "0";
			out += std::format("{:04X}H", static_cast<unsigned int>(opcode[0]));
			break;
		}

		out += mnemonic[instr.operand];
		if (instr.arg1.argtype != U8Decoder::ARG_NONE) {
			out += " " + ParseArg(instr.arg1);
			if (instr.arg2.argtype != U8Decoder::ARG_NONE) {
				out += instr.arg2.argtype == U8Decoder::ARG_BIT_OFFSET ? "." : ", ";
				out += ParseArg(instr.arg2);
			}
		}
		break;
	}

	if (!used_prefix && !dsr_prefix.empty()) {
		ins_len = 2;
		return dsr_prefix == "DSR" ? "EDSR" : "DSR <- " + dsr_prefix;
	}

	return out;
}
