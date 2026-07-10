#pragma once

#include "emxu8.h"
#include <coroutine>
#include <exception>

namespace emxu8 {
	class U8Core;

	struct InstrTask {
		struct promise_type {
			InstrTask get_return_object() { return InstrTask{std::coroutine_handle<promise_type>::from_promise(*this)}; }
			std::suspend_always initial_suspend() { return {}; }
			std::suspend_always final_suspend() noexcept { return {}; }
			std::suspend_always yield_value(unsigned int) { return {}; }
			void return_void() {}
			void unhandled_exception() { std::terminate(); }
		};
		std::coroutine_handle<promise_type> handle;
	};

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
		bool task_running = false;
	private:
		InstrTask ExecuteLoop();
		InstrTask current_task;
		unsigned int current_task_cycle_count;
		bool resuming = false;

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
		friend class U8Core;
	};
}
