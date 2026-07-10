#include "emxu8/emxu8.h"
#include <stdexcept>
#include <cstring>
#include <memory>

using namespace emxu8;

U8Core::U8Core(uint16_t romwin_end) : csr(0),
	romwin_end(romwin_end),
	fetcher(std::make_unique<U8Fetcher>(this)),
	decoder(std::make_unique<U8Decoder>(this)),
	executor(std::make_unique<U8Executor>(this)),
	interrupter(std::make_unique<U8Interrupter>(this))
{
	RegisterSFR(0, 1,
	            [](U8Core *core, uint16_t) { return core->dsr; },
	            [](U8Core *core, uint16_t, uint8_t val) { core->dsr = val; }
	);
}

uint16_t U8Core::GetER(uint8_t n) const {
	n &= ~1;
	if (n > 15) throw std::invalid_argument("n out of range");
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	uint16_t v;
	std::memcpy(&v, &r[n], sizeof(v));
	return v;
#else
	return static_cast<uint16_t>(r[n]) | static_cast<uint16_t>(r[n + 1]) << 8;
#endif
}

void U8Core::SetER(uint8_t n, const uint16_t v) {
	n &= ~1;
	if (n > 15) throw std::invalid_argument("n out of range");
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	std::memcpy(&r[n], &v, sizeof(v));
#else
	r[n] = v;
	r[n + 1] = v >> 8;
#endif
}

uint32_t U8Core::GetXR(uint8_t n) const {
	n &= ~0b11;
	if (n > 15) throw std::invalid_argument("n out of range");
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	uint32_t v;
	std::memcpy(&v, &r[n], sizeof(v));
	return v;
#else
	return static_cast<uint32_t>(r[n]) | static_cast<uint32_t>(r[n + 1]) << 8 | static_cast<uint32_t>(r[n + 2]) << 16 | static_cast<uint32_t>(r[n + 3]) << 24;
#endif
}

void U8Core::SetXR(uint8_t n, const uint32_t v) {
	n &= ~0b11;
	if (n > 15) throw std::invalid_argument("n out of range");
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	std::memcpy(&r[n], &v, sizeof(v));
#else
	r[n] = v;
	r[n + 1] = v >> 8;
	r[n + 2] = v >> 16;
	r[n + 3] = v >> 24;
#endif
}

uint64_t U8Core::GetQR(uint8_t n) const {
	n &= ~0b111;
	if (n > 15) throw std::invalid_argument("n out of range");
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	uint64_t v;
	std::memcpy(&v, &r[n], sizeof(v));
	return v;
#else
	return static_cast<uint64_t>(r[n]) | static_cast<uint64_t>(r[n + 1]) << 8 | static_cast<uint64_t>(r[n + 2]) << 16 | static_cast<uint64_t>(r[n + 3]) << 24 | static_cast<uint64_t>(r[n + 4]) << 32 | static_cast<uint64_t>(r[n + 5]) << 40 | static_cast<uint64_t>(r[n + 6]) << 48 | static_cast<uint64_t>(r[n + 7]) << 56;
#endif
}

void U8Core::SetQR(uint8_t n, const uint64_t v) {
	n &= ~0b111;
	if (n > 15) throw std::invalid_argument("n out of range");
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	std::memcpy(&r[n], &v, sizeof(v));
#else
	r[n] = v;
	r[n + 1] = v >> 8;
	r[n + 2] = v >> 16;
	r[n + 3] = v >> 24;
	r[n + 4] = v >> 32;
	r[n + 5] = v >> 40;
	r[n + 6] = v >> 48;
	r[n + 7] = v >> 56;
#endif
}

uint8_t U8Core::GetECSR() const {
	return ecsr[psw.elevel];
}

void U8Core::SetECSR(uint8_t v) {
	ecsr[psw.elevel] = v;
}

uint16_t U8Core::GetELR() const {
	return elr[psw.elevel];
}

void U8Core::SetELR(uint16_t v) {
	elr[psw.elevel] = v;
}

uint8_t U8Core::GetEPSW() const {
	return epsw[psw.elevel];
}

void U8Core::SetEPSW(uint8_t v) {
	epsw[psw.elevel] = v;
}

void U8Core::RegisterSFR(const uint16_t addr, const uint16_t size, const sfr_read read_callback, const sfr_write write_callback) {
	sfr_read _read_callback = read_callback ? read_callback : NoRead;
	sfr_write _write_callback = write_callback ? write_callback : NoWrite;

	for (uint16_t i = 0; i < size; i++) {
		sfrs_read[addr + i] = _read_callback;
		sfrs_write[addr + i] = _write_callback;
	}
}

std::optional<U8Core::code_region> U8Core::GetCodeRegion(uint16_t addr, uint8_t segment) {
	for (code_region& region : code_regions)
		if (
			region.segment == segment
			&& region.start <= addr && addr < region.start + region.size
		) return region;

	return std::nullopt;
}

std::optional<U8Core::data_region> U8Core::GetDataRegion(uint16_t addr, uint8_t segment) {
	for (data_region& region : data_regions)
		if (
			region.segment == segment
			&& region.start <= addr && addr < region.start + region.size
		) return region;

	return std::nullopt;
}

uint16_t U8Core::ReadCodeMemory(const uint16_t addr, const uint8_t segment) {
	uint8_t *memory = nullptr;
	uint16_t _addr = addr & 0xfffe;

	if (auto region = GetCodeRegion(addr, segment)) {
		memory = region->memory;
		_addr -= region->start;
	}

	if (!memory) return 0xffff;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	uint16_t v;
	std::memcpy(&v, &memory[_addr], sizeof(v));
	return v;
#else
	return static_cast<uint16_t>(memory[_addr]) | static_cast<uint16_t>(memory[_addr + 1]) << 8;
#endif
}

int U8Core::ReadDataMemory(uint16_t addr, const uint8_t segment, uint16_t size, uint8_t *out) {
	int romwin_cycles = 0;

	if (segment == 0 && addr >= 0xf000) {
		for (uint16_t i = 0; i < size; i++) {
			if (addr + i < 0xf000) {
				addr += i;
				size -= i;
				out += i;
				break;
			}
			uint16_t a = addr - 0xf000 + i;
			out[i] = sfrs_read[a] ? sfrs_read[a](this, a) : sfrs[a];
		}
		return 0;
	}

	uint8_t *memory = nullptr;
	uint16_t _addr = addr;
	uint32_t _size = 0;
	bool read = false;

	if (auto region = GetDataRegion(addr, segment)) {
		memory = region->memory;
		_addr -= region->start;
		_size = region->size;
		read = region->read;
	}
	if (!memory) {
		memset(out, 0, size);
		return 0;
	}
	if (segment == 0 && addr < romwin_end) {
		romwin_cycles = size;
		if (addr + size >= romwin_end) romwin_cycles = romwin_end - addr;
	}
	for (int i = 0; i < size; i++) {
		if (_addr + i >= _size) {
			romwin_cycles += ReadDataMemory(addr + i, segment, size - i, &out[i]);
			break;
		}
		out[i] = read ? memory[_addr + i] : 0;
	}
	return romwin_cycles;
}

int U8Core::WriteDataMemory(uint16_t addr, const uint8_t segment, uint16_t size, const uint8_t *in) {
	int romwin_cycles = 0;

	if (segment == 0 && addr >= 0xf000) {
		for (uint16_t i = 0; i < size; i++) {
			if (addr + i < 0xf000) {
				addr += i;
				size -= i;
				in += i;
				break;
			}
			uint16_t a = addr - 0xf000 + i;
			if (sfrs_write[a]) sfrs_write[a](this, a, in[i]);
		}
		return 0;
	}

	uint8_t *memory = nullptr;
	uint16_t _addr = addr;
	uint32_t _size = 0;
	bool write = false;

	if (auto region = GetDataRegion(addr, segment)) {
		memory = region->memory;
		_addr -= region->start;
		_size = region->size;
		write = region->write;
	}

	if (memory) {
		if (segment == 0 && addr < romwin_end) {
			romwin_cycles = size;
			if (addr + size >= romwin_end) romwin_cycles = romwin_end - addr;
		}
		for (int i = 0; i < size; i++) {
			if (_addr + i >= _size) {
				romwin_cycles += WriteDataMemory(addr + i, segment, size - i, &in[i]);
				break;
			}
			if (write) memory[_addr + i] = in[i];
		}
	}
	return romwin_cycles;
}

void U8Core::AddCodeRegion(code_region region) {
	if (!region.memory) throw std::invalid_argument("null memory pointer");
	if (!region.size) throw std::invalid_argument("size must be nonzero");
	if (region.start + region.size > 0x10000) throw std::invalid_argument("region cannot span across multiple segments");
	if (!code_regions.empty()) {
		for (code_region& r : code_regions)
			if (
				r.segment == region.segment
				&& (r.start + r.size <= region.start || region.start + region.size <= r.start)
			) throw std::invalid_argument("code region conflict");
	}
	code_regions.push_back(region);
}

void U8Core::AddDataRegion(data_region region) {
	if (!region.memory) throw std::invalid_argument("null memory pointer");
	if (!region.size) throw std::invalid_argument("size must be nonzero");
	if (static_cast<uint32_t>(region.start + region.size) > 0x10000) throw std::invalid_argument("region cannot span across multiple segments");
	if (!data_regions.empty()) {
		for (data_region& r : data_regions)
			if (
				r.segment == region.segment
				&& !(r.start + r.size <= region.start || region.start + region.size <= r.start)
			) throw std::invalid_argument("data region conflict");
	}
	if (
		region.segment == 0
		&& (region.start >= 0xf000 || region.start + region.size >= 0xf000)
	) throw std::invalid_argument("data region conflict");
	data_regions.push_back(region);
}

void U8Core::SetMemoryModel(MemoryModel model) {
	memory_model = model;
}

void U8Core::RegisterCoprocessor(U8Coprocessor *copro) {
	if (coprocessor) throw std::runtime_error("coprocessor already present");
	if (!copro) throw std::invalid_argument("null memory pointer");
	coprocessor = copro;
}

void U8Core::RegisterPeripheral(U8Peripheral *obj) {
	peripherals.push_back(obj);
}

void U8Core::Reset() {
	{
		std::lock_guard lock(mutex);
		memset(r, 0, sizeof(r));
		pc = ReadCodeMemory(2, 0);
		csr = 0;
		memset(elr, 0, sizeof(elr));
		memset(ecsr, 0, sizeof(ecsr));
		memset(epsw, 0, sizeof(epsw));
		sp = ReadCodeMemory(0, 0);
		ea = 0;
		dsr = 0;
		ClearPipeline();
	}
	cycle_count = 0;
	interrupter->mask_int = true;
	interrupter->callstack.clear();
	if (coprocessor) coprocessor->Reset();
	for (auto &p : peripherals) p->Reset();
	active = true;
}

void U8Core::RequestReset() {
	reset_requested = true;
}


unsigned int U8Core::Tick(const bool *run_cond) {
	if (reset_requested) {
		Reset();
		reset_requested = false;
		return 0;
	}
	unsigned int cycles = 0;
	std::lock_guard lock(mutex);
	pc &= 0xfffe;
	for (auto &p : peripherals) cycles += p->Tick();
	bool tick = run_cond ? *run_cond : true;
	if (active && tick) {
		if (executor->task_running) {
			executor->Tick();
			cycles = 1;
		} else {
			fetcher->Tick();
			prev_csr_pc = executor->cur_pc;
			decoder->Tick();
			executor->Tick();
			cycles = 1;
		}
	}
	if (coprocessor) cycles += coprocessor->Tick();
	cycle_count += cycles;
	return cycles;
}

void U8Core::ClearPipeline() {
	fetcher->Reset();
	decoder->Reset();
	executor->Reset();
	executor->cur_pc = csr << 16 | pc;
	if (!GetCodeRegion(pc, csr)) printf("[U8Core] WARNING: Jump to unmapped code address (%05X -> %05X)\n", prev_csr_pc, csr << 16 | pc);
	else if (csr == 0 && pc < 6) printf("[U8Core] WARNING: Jump to vector table (%05X -> %05X)\n", prev_csr_pc, csr << 16 | pc);
}
