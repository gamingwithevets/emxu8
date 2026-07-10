#include "emxu8/interrupter.h"
#include "emxu8/emxu8.h"
#include <iostream>

using namespace emxu8;

U8Interrupter::U8Interrupter(U8Core *core) : core(core) {}

static void WriteIE(U8Core *core, uint16_t addr, uint8_t val) {
	uint8_t final = 0;
	for (uint8_t i = 0; i < 8; i++) {
		uint16_t vector_addr = core->interrupter->GetIEVectorAddress(addr, i);
		if (val & 1 << i && vector_addr >= 6 && vector_addr < 0x80) final |= 1 << i;
	}
	core->sfrs[addr] = final;
}

static void WriteIRQ(U8Core *core, uint16_t addr, uint8_t val) {
	core->sfrs[addr] = val;
	for (uint8_t i = 0; i < 8; i++) {
		uint16_t idx = addr - core->interrupter->irq_start;
		uint16_t vector_addr = core->interrupter->GetIRQVectorAddress(addr, i);
		if (val & 1 << i && vector_addr >= 6 && vector_addr < 0x80) core->interrupter->TryRaiseInterrupt(idx, i);
	}
}

void U8Interrupter::AllocateSFR(uint16_t start, uint16_t size) {
	if (start >= 0x1000) throw std::invalid_argument("start address out of range");
	if (start + size >= 0x1000) throw std::invalid_argument("size out of range");
	ie_start = start;
	irq_start = start + size;
	core->RegisterSFR(ie_start, size, U8Core::DefaultRead<0xff>, WriteIE);
	core->RegisterSFR(irq_start, size, U8Core::DefaultRead<0xff>, WriteIRQ);
}

void U8Interrupter::TryRaiseInterrupt(const uint16_t idx, const uint8_t bit) {
	bool cond;
	uint8_t elevel;
	uint16_t vector_addr;
	interrupt intr;
	switch (bit) {
		case 9:
			intr = {4, "BRK"};
			vector_addr = 4;
			cond = true;
			elevel = 2;
			break;
		case 8:
			vector_addr = 0x80 + idx * 2;
			intr = {vector_addr, std::format("SWI #{}", static_cast<unsigned int>(idx))};
			cond = true;
			elevel = 1;
			break;
		default:
			cond = core->sfrs[ie_start + idx] & 1 << bit && !mask_int;
			intr = interrupts[idx][bit];
			vector_addr = intr.vector_addr;
			if (vector_addr == 6) {
				cond = cond && core->psw.elevel <= 3;
				elevel = 3;
			} else if (vector_addr == 8) {
				cond = cond && core->psw.elevel <= 2;
				elevel = 2;
			} else {
				cond = cond && core->psw.mie && core->psw.elevel <= 1;
				elevel = 1;
			}
			break;
	}
	if (cond) {
		mask_int = true;
		core->elr[elevel] = core->executor->next_pc;
		core->ecsr[elevel] = core->csr;
		core->epsw[elevel] = core->psw.raw;
		core->psw.elevel = elevel;
		if (elevel == 1) core->psw.mie = false;
		core->csr = 0;
		core->pc = core->ReadCodeMemory(vector_addr, 0);
		if (bit < 8) {
			core->sfrs[irq_start + idx] &= ~(1 << bit);
			core->cycle_count += 3;
		}
		callstack.push_back({static_cast<uint32_t>(core->csr << 16 | core->pc), static_cast<uint32_t>(core->ecsr[elevel] << 16 | core->elr[elevel]), 0, intr});
		core->ClearPipeline();
	} else core->sfrs[irq_start + idx] |= 1 << bit;
	if (core->sfrs[ie_start + idx] & 1 << bit) core->active = true;
}

void U8Interrupter::AddInterrupt(const uint16_t idx, const uint8_t bit, uint16_t vector_addr, const std::string &name) {
	if (bit > 7) throw std::invalid_argument("bit out of range");
	vector_addr &= ~1;
	if (vector_addr < 6 || vector_addr > 0x7f) throw std::invalid_argument("vector address out of range");
	if (interrupts[idx][bit].vector_addr != 0) throw std::invalid_argument("interrupt already exists at this location");
	interrupts[idx][bit] = {vector_addr, name};
}

uint16_t U8Interrupter::GetIEVectorAddress(const uint16_t addr, const uint8_t bit) {
	return interrupts[addr - ie_start][bit].vector_addr;
}

uint16_t U8Interrupter::GetIRQVectorAddress(const uint16_t addr, const uint8_t bit) {
	return interrupts[addr - irq_start][bit].vector_addr;
}
