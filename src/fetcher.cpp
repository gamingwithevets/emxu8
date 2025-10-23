#include "emxu8/fetcher.h"
#include "emxu8/emxu8.h"

using namespace emxu8;

U8Fetcher::U8Fetcher(U8Core *core) : core(core) {}

void U8Fetcher::Tick() {
	if (active) {
		if (fetches.size() > 5) fetches.erase(fetches.begin());
		fetches.insert({core->csr << 16 | core->pc, core->ReadCodeMemory(core->pc, core->csr)});
		core->pc += 2;
	}
}

void U8Fetcher::Reset() {
	fetches.clear();
	active = true;
}
