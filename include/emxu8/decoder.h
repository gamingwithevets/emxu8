#pragma once

#include "decoder.h"
#include "emxu8.h"
#include <map>
#include <cstdint>

namespace emxu8 {
	class U8Core;

	class U8Decoder {
		U8Core *core;

	public:
		enum InstrOp {
			OP_NONE = 0,

			// Arithmetic Instructions
			OP_ADD,
			OP_ADDC,
			OP_AND,
			OP_CMP,
			OP_CMPC,
			OP_MOV,
			OP_OR,
			OP_XOR,
			OP_SUB,
			OP_SUBC,

			// Shift Instructions
			OP_SLL,
			OP_SLLC,
			OP_SRA,
			OP_SRL,
			OP_SRLC,

			// Load/Store Instructions
			OP_L,
			OP_ST,

			// Control Register Access Instructions (ADD, MOV already included)

			// PUSH/POP Instructions
			OP_PUSH,
			OP_POP,

			// Coprocessor Data Transfer Instructions (mov already included)

			// EA Register Data Transfer Instructions
			OP_LEA,

			// ALU Instructions
			OP_DAA,
			OP_DAS,
			OP_NEG,

			// Bit Access Instructions
			OP_SB,
			OP_RB,
			OP_TB,

			// PSW Access Instructions
			OP_EI,
			OP_DI,
			OP_SC,
			OP_RC,
			OP_CPLC,

			// Conditional Relative Branch Instructions
			OP_BC,

			// Sign Extension Instruction
			OP_EXTBW,

			// Software Interrupt Instructions
			OP_SWI,
			OP_BRK,

			// Branch Instructions
			OP_B,
			OP_BL,

			// Multiplication and Division Instructions
			OP_MUL,
			OP_DIV,

			// Miscellaneous
			OP_INC,
			OP_DEC,
			OP_RT,
			OP_RTI,
			OP_NOP,

			// DSR Prefix Instructions
			OP_DSR,
		};

		enum InstrArg {
			/*
No argument.
* Flags:   None
* Value:   None
			*/
			ARG_NONE = 0,
			/*
Register.
* Flags:  Register size (1/2/4/8)
* Value:  n
			*/
			ARG_REG,
			/*
Number.
* Flags:  Number bit size
* Value:  Number value
			 */
			ARG_NUM,
			/*
[EA]/[EA+].
* Flags:  Toggle [EA+] (bool)
* Value:  None
			*/
			ARG_PTR_EA,
			/*
[ERm].
* Flags:  Toggle Disp16 (bool)
* Value:  m
			*/
			ARG_PTR_REG,
			/*
Disp6[BP] / Disp6[FP].
* Flags:  Toggle FP/BP (bool); 0 = FP
* Value:  Disp6
			*/
			ARG_PTR_BFP_DISP,
			/*
Address.
* Flags:  Type (0: Cadr, 1: Dadr, 2: Radr)
* Value:  Cadr: Segment number, Dadr: None, Radr: Jump word count
			*/
			ARG_ADR,
			/*
Control register.
* Flags:  Register (0: SP, 1: ECSR, 2: ELR, 3: EPSW, 4: PSW)
* Value:  None
			*/
			ARG_CONREG,
			/*
Register list for PUSH instruction.
* Flags:  None
* Value:  Register list bits
			*/
			ARG_PUSH_LIST,
			/*
Register list for POP instruction.
* Flags:  None
* Value:  Register list bits
			*/
			ARG_POP_LIST,
			/*
Coprocessor register.
* Flags:  Register size (1/2/4/8)
* Value:  n
			*/
			ARG_REG_C,

			/*
Arbitrary number type.
* Flags:  None
* Value:  Value
			*/
			ARG_BIT_OFFSET,
			/*
Branch condition.
* Flags:  None
* Value:  Condition enum
			*/
			ARG_COND,
		};

		struct instr_arg {
			InstrArg argtype;
			uint8_t flags;
			uint8_t value;
		};

		struct instruction {
			uint16_t opcode;
			InstrOp operand;
			instr_arg arg1;
			instr_arg arg2;
		};
		
	private:
		struct instr_arg_raw {
			InstrArg argtype;
			uint8_t flags;
			uint16_t mask;
			uint8_t shift;
		};

		struct instruction_raw {
			InstrOp operand;
			uint16_t opcode;
			uint16_t mask;
			instr_arg_raw arg1;
			instr_arg_raw arg2;
		};

		static constexpr instruction_raw instructions[] = {
			// Arithmetic Instructions
			{OP_ADD,	0x8001, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// ADD		Rn,		Rm
			{OP_ADD,	0x1000, 0xf000, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x08, 0x00ff, 0}},	// ADD		Rn,		#imm8
			{OP_ADD,	0xf006, 0xf11f, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_REG,			0x02, 0x00e0, 4}},	// ADD		ERn,	ERm
			{OP_ADD,	0xe080, 0xf180, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_NUM,			0x07, 0x007f, 0}},	// ADD		ERn,	#imm7
			{OP_ADDC,	0x8006, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// ADDC		Rn,		Rm
			{OP_ADDC,	0x6000, 0xf000, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x08, 0x00ff, 0}},	// ADDC		Rn,		#imm8
			{OP_AND,	0x8002, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// AND		Rn,		Rm
			{OP_AND,	0x2000, 0xf000, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x08, 0x00ff, 0}},	// AND		Rn,		#imm8
			{OP_CMP,	0x8007, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// CMP		Rn,		Rm
			{OP_CMP,	0x7000, 0xf000, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x08, 0x00ff, 0}},	// CMP		Rn,		#imm8
			{OP_CMPC,	0x8005, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// CMPC		Rn,		Rm
			{OP_CMPC,	0x5000, 0xf000, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x08, 0x00ff, 0}},	// CMPC		Rn,		#imm8
			{OP_MOV,	0x8000, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// MOV		Rn,		Rm
			{OP_MOV,	0x0000, 0xf000, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x08, 0x00ff, 0}},	// MOV		Rn,		#imm8
			{OP_MOV,	0xf005, 0xf11f, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_REG,			0x02, 0x00e0, 4}},	// MOV		ERn,	ERm
			{OP_MOV,	0xe000, 0xf180, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_NUM,			0x07, 0x007f, 0}},	// MOV		ERn,	#imm7
			{OP_OR,		0x8003, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// OR		Rn,		Rm
			{OP_OR,		0x3000, 0xf000, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x08, 0x00ff, 0}},	// OR		Rn,		#imm8
			{OP_XOR,	0x8004, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// XOR		Rn,		Rm
			{OP_XOR,	0x4000, 0xf000, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x08, 0x00ff, 0}},	// XOR		Rn,		#imm8
			{OP_CMP,	0xf007, 0xf11f, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_REG,			0x02, 0x00e0, 4}},	// CMP		ERn,	ERm
			{OP_SUB,	0x8008, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// SUB		Rn,		Rm
			{OP_SUBC,	0x8009, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// SUBC		Rn,		Rm

			// Shift Instructions
			{OP_SLL,	0x800a, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// SLL		Rn,		Rm
			{OP_SLL,	0x900a, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x03, 0x0070, 4}},	// SLL		Rn,		#width
			{OP_SLLC,	0x800b, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// SLLC		Rn,		Rm
			{OP_SLLC,	0x900b, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x03, 0x0070, 4}},	// SLLC		Rn,		#width
			{OP_SRA,	0x800e, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// SRA		Rn,		Rm
			{OP_SRA,	0x900e, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x03, 0x0070, 4}},	// SRA		Rn,		#width
			{OP_SRL,	0x800c, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// SRL		Rn,		Rm
			{OP_SRL,	0x900c, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x03, 0x0070, 4}},	// SRL		Rn,		#width
			{OP_SRLC,	0x800d, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// SRLC		Rn,		Rm
			{OP_SRLC,	0x900d, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_NUM,			0x03, 0x0070, 4}},	// SRLC		Rn,		#width

			// Load/Store Instructions
			{OP_L,		0x9032, 0xf1ff, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_PTR_EA}},							// L		ERn,	[EA]
			{OP_L,		0x9052, 0xf1ff, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_PTR_EA,		0x01}},				// L		ERn,	[EA+]
			{OP_L,		0x9002, 0xf11f, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_PTR_REG,		0x00, 0x00e0, 4}},	// L		ERn,	[ERm]
			{OP_L,		0xa008, 0xf11f, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_PTR_REG,		0x01, 0x00e0, 4}},	// L		ERn,	Disp16[ERm]
			{OP_L,		0xb000, 0xf1c0, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_PTR_BFP_DISP,	0x01, 0x003f, 0}},	// L		ERn,	Disp6[BP]
			{OP_L,		0xb040, 0xf1c0, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_PTR_BFP_DISP,	0x00, 0x003f, 0}},	// L		ERn,	Disp6[FP]
			{OP_L,		0x9012, 0xf1ff, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_ADR,			0x01}},				// L		ERn,	Dadr
			{OP_L,		0x9030, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_PTR_EA}},							// L		Rn,		[EA]
			{OP_L,		0x9050, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_PTR_EA,		0x01}},				// L		Rn,		[EA+]
			{OP_L,		0x9000, 0xf01f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_PTR_REG,		0x00, 0x00e0, 4}},	// L		Rn,		[ERm]
			{OP_L,		0x9008, 0xf01f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_PTR_REG,		0x01, 0x00e0, 4}},	// L		Rn,		Disp16[ERm]
			{OP_L,		0xd000, 0xf0c0, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_PTR_BFP_DISP,	0x01, 0x003f, 0}},	// L		Rn,		Disp6[BP]
			{OP_L,		0xd040, 0xf0c0, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_PTR_BFP_DISP,	0x00, 0x003f, 0}},	// L		Rn,		Disp6[FP]
			{OP_L,		0x9010, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_ADR,			0x01}},				// L		Rn,		Dadr
			{OP_L,		0x9034, 0xf3ff, {ARG_REG,			0x04, 0x0c00, 8},	{ARG_PTR_EA}},							// L		XRn,	[EA]
			{OP_L,		0x9054, 0xf3ff, {ARG_REG,			0x04, 0x0c00, 8},	{ARG_PTR_EA,		0x01}},				// L		XRn,	[EA+]
			{OP_L,		0x9036, 0xf7ff, {ARG_REG,			0x08, 0x0800, 8},	{ARG_PTR_EA}},							// L		QRn,	[EA]
			{OP_L,		0x9056, 0xf7ff, {ARG_REG,			0x08, 0x0800, 8},	{ARG_PTR_EA,		0x01}},				// L		QRn,	[EA+]
			{OP_ST,		0x9033, 0xf1ff, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_PTR_EA}},							// ST		ERn,	[EA]
			{OP_ST,		0x9053, 0xf1ff, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_PTR_EA,		0x01}},				// ST		ERn,	[EA+]
			{OP_ST,		0x9003, 0xf11f, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_PTR_REG,		0x00, 0x00e0, 4}},	// ST		ERn,	[ERm]
			{OP_ST,		0xa009, 0xf11f, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_PTR_REG,		0x01, 0x00e0, 4}},	// ST		ERn,	Disp16[ERm]
			{OP_ST,		0xb080, 0xf1c0, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_PTR_BFP_DISP,	0x01, 0x003f, 0}},	// ST		ERn,	Disp6[BP]
			{OP_ST,		0xb0c0, 0xf1c0, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_PTR_BFP_DISP,	0x00, 0x003f, 0}},	// ST		ERn,	Disp6[FP]
			{OP_ST,		0x9013, 0xf1ff, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_ADR,			0x01}},				// ST		ERn,	Dadr
			{OP_ST,		0x9031, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_PTR_EA}},							// ST		Rn,		[EA]
			{OP_ST,		0x9051, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_PTR_EA,		0x01}},				// ST		Rn,		[EA+]
			{OP_ST,		0x9001, 0xf01f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_PTR_REG,		0x00, 0x00e0, 4}},	// ST		Rn,		[ERm]
			{OP_ST,		0x9009, 0xf01f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_PTR_REG,		0x01, 0x00e0, 4}},	// ST		Rn,		Disp16[ERm]
			{OP_ST,		0xd080, 0xf0c0, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_PTR_BFP_DISP,	0x01, 0x003f, 0}},	// ST		Rn,		Disp6[BP]
			{OP_ST,		0xd0c0, 0xf0c0, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_PTR_BFP_DISP,	0x00, 0x003f, 0}},	// ST		Rn,		Disp6[FP]
			{OP_ST,		0x9011, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_ADR,			0x01}},				// ST		Rn,		Dadr
			{OP_ST,		0x9035, 0xf3ff, {ARG_REG,			0x04, 0x0c00, 8},	{ARG_PTR_EA}},							// ST		XRn,	[EA]
			{OP_ST,		0x9055, 0xf3ff, {ARG_REG,			0x04, 0x0c00, 8},	{ARG_PTR_EA,		0x01}},				// ST		XRn,	[EA+]
			{OP_ST,		0x9037, 0xf7ff, {ARG_REG,			0x08, 0x0800, 8},	{ARG_PTR_EA}},							// ST		QRn,	[EA]
			{OP_ST,		0x9057, 0xf7ff, {ARG_REG,			0x08, 0x0800, 8},	{ARG_PTR_EA,		0x01}},				// ST		QRn,	[EA+]

			// Control Register Access Instructions
			{OP_ADD,	0xe100, 0xff00, {ARG_CONREG,		0x00},				{ARG_NUM,			0x08, 0x00ff, 0}},	// ADD		SP,		#signed8
			{OP_MOV,	0xa00f, 0xff0f, {ARG_CONREG,		0x01},				{ARG_REG,			0x01, 0x00f0, 4}},	// MOV		ECSR,	Rm
			{OP_MOV,	0xa00d, 0xf1ff, {ARG_CONREG,		0x02},				{ARG_REG,			0x02, 0x0e00, 8}},	// MOV		ELR,	ERm
			{OP_MOV,	0xa00c, 0xff0f, {ARG_CONREG,		0x03},				{ARG_REG,			0x01, 0x00f0, 4}},	// MOV		EPSW,	Rm
			{OP_MOV,	0xa005, 0xf1ff, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_CONREG,		0x02}},				// MOV		ERn,	ELR
			{OP_MOV,	0xa01a, 0xf0ff, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_CONREG,		0x00}},				// MOV		ERn,	SP
			{OP_MOV,	0xa00b, 0xff0f, {ARG_CONREG,		0x04},				{ARG_REG,			0x01, 0x00f0, 4}},	// MOV		PSW,	Rm
			{OP_MOV,	0xe900, 0xff00, {ARG_CONREG,		0x04},				{ARG_NUM,			0x08, 0x00ff, 0}},	// MOV		PSW,	#unsigned8
			{OP_MOV,	0xa007, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_CONREG,		0x01}},				// MOV		Rn,		ECSR
			{OP_MOV,	0xa004, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_CONREG,		0x03}},				// MOV		Rn,		EPSW
			{OP_MOV,	0xa003, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_CONREG,		0x04}},				// MOV		Rn,		PSW
			{OP_MOV,	0xa10a, 0xff1f, {ARG_CONREG,		0x00},				{ARG_REG,			0x02, 0x00e0, 4}},	// MOV		SP,		ERm

			// PUSH/POP Instructions
			{OP_PUSH,	0xf05e, 0xf1ff, {ARG_REG,			0x02, 0x0e00, 8}},											// PUSH		ERn
			{OP_PUSH,	0xf07e, 0xf7ff, {ARG_REG,			0x08, 0x0800, 8}},											// PUSH		QRn
			{OP_PUSH,	0xf04e, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8}},											// PUSH		Rn
			{OP_PUSH,	0xf06e, 0xf3ff, {ARG_REG,			0x04, 0x0c00, 8}},											// PUSH		XRn
			{OP_PUSH,	0xf0ce, 0xf0ff, {ARG_PUSH_LIST,		0x00, 0x0f00, 8}},											// PUSH		register_list
			{OP_POP,	0xf01e, 0xf1ff, {ARG_REG,			0x02, 0x0e00, 8}},											// POP		ERn
			{OP_POP,	0xf03e, 0xf7ff, {ARG_REG,			0x08, 0x0800, 8}},											// POP		QRn
			{OP_POP,	0xf00e, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8}},											// POP		Rn
			{OP_POP,	0xf02e, 0xf3ff, {ARG_REG,			0x04, 0x0c00, 8}},											// POP		XRn
			{OP_POP,	0xf08e, 0xf0ff, {ARG_POP_LIST,		0x00, 0x0f00, 8}},											// POP		register_list

			// Coprocessor Data Transfer Instructions
			{OP_MOV,	0xa00e, 0xf00f, {ARG_REG_C,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// MOV		CRn,	Rm
			{OP_MOV,	0xf02d, 0xf1ff, {ARG_REG_C,			0x02, 0x0e00, 8},	{ARG_PTR_EA}},							// MOV		CERn,	[EA]
			{OP_MOV,	0xf03d, 0xf1ff, {ARG_REG_C,			0x02, 0x0e00, 8},	{ARG_PTR_EA,		0x01}},				// MOV		CERn,	[EA+]
			{OP_MOV,	0xf00d, 0xf0ff, {ARG_REG_C,			0x01, 0x0f00, 8},	{ARG_PTR_EA}},							// MOV		CRn,	[EA]
			{OP_MOV,	0xf01d, 0xf0ff, {ARG_REG_C,			0x01, 0x0f00, 8},	{ARG_PTR_EA,		0x01}},				// MOV		CRn,	[EA+]
			{OP_MOV,	0xf04d, 0xf3ff, {ARG_REG_C,			0x04, 0x0c00, 8},	{ARG_PTR_EA}},							// MOV		CXRn,	[EA]
			{OP_MOV,	0xf05d, 0xf3ff, {ARG_REG_C,			0x04, 0x0c00, 8},	{ARG_PTR_EA,		0x01}},				// MOV		CXRn,	[EA+]
			{OP_MOV,	0xf06d, 0xf7ff, {ARG_REG_C,			0x08, 0x0800, 8},	{ARG_PTR_EA}},							// MOV		CQRn,	[EA]
			{OP_MOV,	0xf07d, 0xf7ff, {ARG_REG_C,			0x08, 0x0800, 8},	{ARG_PTR_EA,		0x01}},				// MOV		CQRn,	[EA+]
			{OP_MOV,	0xa006, 0xf00f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG_C,			0x01, 0x00f0, 4}},	// MOV		Rn,		CRm
			{OP_MOV,	0xf0ad, 0xf1ff, {ARG_PTR_EA},							{ARG_REG_C,			0x02, 0x0e00, 8}},	// MOV		[EA],	CERm
			{OP_MOV,	0xf0bd, 0xf1ff, {ARG_PTR_EA,		0x01},				{ARG_REG_C,			0x02, 0x0e00, 8}},	// MOV		[EA+],	CERm
			{OP_MOV,	0xf08d, 0xf0ff, {ARG_PTR_EA},							{ARG_REG_C,			0x01, 0x0f00, 8}},	// MOV		[EA],	CRm
			{OP_MOV,	0xf09d, 0xf0ff, {ARG_PTR_EA,		0x01},				{ARG_REG_C,			0x01, 0x0f00, 8}},	// MOV		[EA+],	CRm
			{OP_MOV,	0xf0cd, 0xf3ff, {ARG_PTR_EA},							{ARG_REG_C,			0x04, 0x0c00, 8}},	// MOV		[EA],	CXRm
			{OP_MOV,	0xf0dd, 0xf3ff, {ARG_PTR_EA,		0x01},				{ARG_REG_C,			0x04, 0x0c00, 8}},	// MOV		[EA+],	CXRm
			{OP_MOV,	0xf0ed, 0xf7ff, {ARG_PTR_EA},							{ARG_REG_C,			0x08, 0x0800, 8}},	// MOV		[EA],	CQRm
			{OP_MOV,	0xf0fd, 0xf7ff, {ARG_PTR_EA,		0x01},				{ARG_REG_C,			0x08, 0x0800, 8}},	// MOV		[EA+],	CQRm

			// EA Register Data Transfer Instructions
			{OP_LEA,	0xf00a, 0xff1f, {ARG_PTR_REG,		0x00, 0x00e0, 4}},											// LEA		[ERm]
			{OP_LEA,	0xf00b, 0xff1f, {ARG_PTR_REG,		0x01, 0x00e0, 4}},											// LEA		Disp16[ERm]
			{OP_LEA,	0xf00c, 0xffff, {ARG_ADR,			0x01}},														// LEA		Dadr

			// ALU Instructions
			{OP_DAA,	0x801f, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8}},											// DAA		Rn
			{OP_DAS,	0x803f, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8}},											// DAS		Rn
			{OP_NEG,	0x805f, 0xf0ff, {ARG_REG,			0x01, 0x0f00, 8}},											// NEG		Rn

			// Bit Access Instructions
			{OP_SB,		0xa000, 0xf08f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_BIT_OFFSET,	0x00, 0x0070, 4}},	// SB		Rn.bit_offset
			{OP_SB,		0xa080, 0xf08f, {ARG_ADR,			0x01},				{ARG_BIT_OFFSET,	0x00, 0x0070, 4}},	// SB		Dbitadr
			{OP_RB,		0xa002, 0xf08f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_BIT_OFFSET,	0x00, 0x0070, 4}},	// RB		Rn.bit_offset
			{OP_RB,		0xa082, 0xf08f, {ARG_ADR,			0x01},				{ARG_BIT_OFFSET,	0x00, 0x0070, 4}},	// RB		Dbitadr
			{OP_TB,		0xa001, 0xf08f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_BIT_OFFSET,	0x00, 0x0070, 4}},	// RB		Rn.bit_offset
			{OP_TB,		0xa081, 0xf08f, {ARG_ADR,			0x01},				{ARG_BIT_OFFSET,	0x00, 0x0070, 4}},	// RB		Dbitadr

			// PSW Access Instructions
			{OP_EI,		0xed08, 0xffff},																				// EI
			{OP_DI,		0xebf7, 0xffff},																				// DI
			{OP_SC,		0xed80, 0xffff},																				// SC
			{OP_RC,		0xeb7f, 0xffff},																				// RC
			{OP_CPLC,	0xfecf, 0xffff},																				// CPLC

			// Conditional Relative Branch Instructions
			{OP_BC,		0xc000, 0xf000, {ARG_COND,			0x00, 0x0f00, 8},	{ARG_ADR,			0x02, 0x00ff, 0}},	// BC		cond,	Radr

			// Sign Extension Instruction
			{OP_EXTBW,	0x810f, 0xf11f, {ARG_REG,			0x01, 0x0f00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// EXTBW	ERn

			// Software Interrupt Instructions
			{OP_SWI,	0xe500, 0xf0ff, {ARG_NUM,			0x06, 0x003f, 0}},											// SWI		#snum
			{OP_BRK,	0xffff, 0xffff},																				// BRK

			// Branch Instructions
			{OP_B,		0xf000, 0xf0ff, {ARG_ADR,			0x00, 0x0f00, 8}},											// B		Cadr
			{OP_B,		0xf002, 0xff1f, {ARG_REG,			0x02, 0x00e0, 4}},											// B		ERn
			{OP_BL,		0xf001, 0xf0ff, {ARG_ADR,			0x00, 0x0f00, 8}},											// BL		Cadr
			{OP_BL,		0xf003, 0xff1f, {ARG_REG,			0x02, 0x00e0, 4}},											// BL		ERn

			// Multiplication and Division Instructions
			{OP_MUL,	0xf004, 0xf10f, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// MUL		ERn,	Rm
			{OP_DIV,	0xf009, 0xf10f, {ARG_REG,			0x02, 0x0e00, 8},	{ARG_REG,			0x01, 0x00f0, 4}},	// DIV		ERn,	Rm

			// Miscellaneous
			{OP_INC,	0xfe2f,	0xffff},																				// INC		[EA]
			{OP_DEC,	0xfe3f,	0xffff},																				// DEC		[EA]
			{OP_RT,		0xfe1f,	0xffff},																				// RT
			{OP_RTI,	0xfe0f,	0xffff},																				// RTI
			{OP_NOP,	0xfe8f,	0xffff},																				// NOP

			// DSR Prefix Instructions
			{OP_DSR,	0xe300, 0xff00, {ARG_BIT_OFFSET,	0x00, 0x00ff, 0}},											// pseg_addr
			{OP_DSR,	0xfe9f, 0xffff},																				// DSR
			{OP_DSR,	0x900f, 0xff0f, {ARG_REG,			0x01, 0x00f0, 4}},											// Rd
		};

	public:
		explicit U8Decoder(U8Core *core);
		void Tick();
		void Reset();

		std::map<uint32_t, instruction> decodes{};
		bool active = true;
		uint32_t cur_pc = 0x100000;

		friend class U8Executor;
		friend class U8Disassembler;
	};
}
