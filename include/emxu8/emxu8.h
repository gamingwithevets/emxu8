#pragma once

#include <memory>
#include <cstdint>
#include <vector>
#include <mutex>
#include <optional>
#include "compilerdefs.h"
#include "fetcher.h"
#include "decoder.h"
#include "executor.h"
#include "interrupter.h"

#define EMXU8_CORE_VERSION "dev"

namespace emxu8 {
	class U8Fetcher;
	class U8Decoder;
	class U8Executor;
	class U8Interrupter;
	class U8Peripheral;

	class U8Coprocessor {
	public:
		virtual ~U8Coprocessor() = default;

		[[nodiscard]] virtual uint8_t GetR(uint8_t n) const;
		virtual void SetR(uint8_t n, uint8_t v);
		[[nodiscard]] virtual uint16_t GetER(uint8_t n) const;
		virtual void SetER(uint8_t n, uint16_t v);
		[[nodiscard]] virtual uint32_t GetXR(uint8_t n) const;
		virtual void SetXR(uint8_t n, uint32_t v);
		[[nodiscard]] virtual uint64_t GetQR(uint8_t n) const;
		virtual void SetQR(uint8_t n, uint64_t v);

		virtual void Reset() {}
		virtual int Tick() { return 0; }
	};

	enum MemoryModel {
		MM_SMALL,
		MM_LARGE
	};

	class EMXU8_API U8Core {
		struct code_region {
			uint8_t segment : 4;
			uint16_t start;
			uint32_t size;
			uint8_t *memory;
		};

		struct data_region {
			uint8_t segment;
			uint16_t start;
			uint32_t size;
			bool read;
			bool write;
			uint8_t *memory;
		};

		typedef uint8_t (*sfr_read)(U8Core *, uint16_t);

		typedef void (*sfr_write)(U8Core *, uint16_t, uint8_t);

		bool reset_requested = false;

	public:
		U8Core(uint16_t romwin_end);

		[[nodiscard]] uint16_t GetER(uint8_t n) const;
		void SetER(uint8_t n, uint16_t v);
		[[nodiscard]] uint32_t GetXR(uint8_t n) const;
		void SetXR(uint8_t n, uint32_t v);
		[[nodiscard]] uint64_t GetQR(uint8_t n) const;
		void SetQR(uint8_t n, uint64_t v);
		[[nodiscard]] uint8_t GetECSR() const;
		void SetECSR(uint8_t v);
		[[nodiscard]] uint16_t GetELR() const;
		void SetELR(uint16_t v);
		[[nodiscard]] uint8_t GetEPSW() const;
		void SetEPSW(uint8_t v);

		[[nodiscard]] static uint8_t NoRead(U8Core *, uint16_t) { return 0; }
		static void NoWrite(U8Core *, uint16_t, uint8_t) {}

		template <uint8_t mask>
		[[nodiscard]] static uint8_t DefaultRead(U8Core *core, uint16_t addr) { return core->sfrs[addr] & mask; }
		template <uint8_t mask>
		static void DefaultWrite(U8Core *core, uint16_t addr, uint8_t val) { core->sfrs[addr] = val & mask; }

		void RegisterSFR(uint16_t addr, uint16_t size, sfr_read read_callback, sfr_write write_callback);

		std::optional<code_region> GetCodeRegion(uint16_t addr, uint8_t segment);
		std::optional<data_region> GetDataRegion(uint16_t addr, uint8_t segment);
		uint16_t ReadCodeMemory(uint16_t addr, uint8_t segment);
		int ReadDataMemory(uint16_t addr, uint8_t segment, uint16_t size, uint8_t *out);
		int WriteDataMemory(uint16_t addr, uint8_t segment, uint16_t size, const uint8_t *in);

		void AddCodeRegion(code_region region);
		void AddDataRegion(data_region region);

		void SetMemoryModel(MemoryModel model);
		[[nodiscard]] MemoryModel GetMemoryModel() const { return memory_model; }

		void RegisterCoprocessor(U8Coprocessor *copro);
		void RegisterPeripheral(U8Peripheral *obj);

		void Reset();
		void RequestReset();
		unsigned int Tick(const bool *run_cond = nullptr);
		void ClearPipeline();

		bool active = true;

		std::unique_ptr<U8Fetcher> fetcher;
		std::unique_ptr<U8Decoder> decoder;
		std::unique_ptr<U8Executor> executor;
		std::unique_ptr<U8Interrupter> interrupter;

		uint8_t r[16]{};

		uint16_t pc{};
		uint8_t csr : 4;
		union {
			uint16_t lr;
			uint16_t elr[4]{};
		};
		union {
			uint8_t lcsr;
			uint8_t ecsr[4]{};
		};
		union {
			union {
				PACKED_ANON_STRUCT {
					bool c			: 1;
					bool z			: 1;
					bool s			: 1;
					bool ov			: 1;
					bool mie		: 1;
					bool hc			: 1;
					uint8_t elevel	: 2;
				};

				uint8_t raw;
			} psw;
			uint8_t epsw[4]{};
		};
		uint16_t sp{};
		uint16_t ea{};
		uint8_t dsr{};

		uint8_t sfrs[0x1000]{};

		uint32_t prev_csr_pc;

	protected:
		uint16_t romwin_end;
		std::vector<code_region> code_regions{};
		std::vector<data_region> data_regions{};
		sfr_read sfrs_read[0x1000]{};
		sfr_write sfrs_write[0x1000]{};

		MemoryModel memory_model{};

		U8Coprocessor *coprocessor{};
		std::vector<U8Peripheral *> peripherals;

	public:
		unsigned int cycle_count{};
		std::mutex mutex;

	protected:
		template<typename T>
		void StackPush(T value, uint8_t size);
		template<typename T>
		void StackPop(T value, uint8_t size);

		friend class U8Executor;
	};

	class U8Peripheral {
	protected:
		U8Core *core = nullptr;
	public:
		explicit U8Peripheral(U8Core *core) : core(core) { core->RegisterPeripheral(this); }
		virtual ~U8Peripheral() = default;

		virtual void Reset() {}
		virtual unsigned int Tick() { return 0; }
	};
}
