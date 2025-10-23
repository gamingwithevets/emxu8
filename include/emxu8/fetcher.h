#pragma once

#include <map>
#include <cstdint>
#include "emxu8.h"

namespace emxu8 {
	class U8Core;

	class U8Fetcher {
		U8Core *core;
	public:
		explicit U8Fetcher(U8Core *core);
		void Tick();
		void Reset();

		std::map<uint32_t, uint16_t> fetches{};
		bool active = true;
	};
}