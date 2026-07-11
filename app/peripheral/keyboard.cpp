#include "keyboard.h"

Keyboard::Keyboard() {
	core->RegisterSFR(0x40, 1, emxu8::U8Core::DefaultRead<0xff>, emxu8::U8Core::NoWrite);
	core->RegisterSFR(0x41, 2, emxu8::U8Core::DefaultRead<0xff>, emxu8::U8Core::DefaultWrite<0xff>);
	core->RegisterSFR(0x44, 3, emxu8::U8Core::DefaultRead<0xff>, emxu8::U8Core::DefaultWrite<0xff>);
	core->RegisterSFR(0x47, 1, emxu8::U8Core::DefaultRead<0x83>, emxu8::U8Core::DefaultWrite<0x83>);
}

void Keyboard::Reset() {
	memset(&core->sfrs[0x41], 0, 6);
}

void Keyboard::ProcessEvent(SDL_Renderer *renderer, const SDL_Event *e) {
	SDL_Keycode key;
	switch (e->type) {
		case SDL_EVENT_KEY_DOWN:
			key = SDL_GetKeyFromScancode(e->key.scancode, e->key.mod, false);
			for (const auto &[k, v] : mcu->config->keymap) {
				if (std::find(v.keys.begin(), v.keys.end(), key) != v.keys.end()) {
					held_buttons.push_back(k);
					break;
				}
			}
			break;
		case SDL_EVENT_KEY_UP:
			key = SDL_GetKeyFromScancode(e->key.scancode, e->key.mod, false);
			for (const auto &[k, v] : mcu->config->keymap) {
				if (std::find(v.keys.begin(), v.keys.end(), key) != v.keys.end()) {
					std::vector<uint8_t>::iterator pos = std::find(held_buttons.begin(), held_buttons.end(), k);
					if (pos != held_buttons.end()) {
						held_buttons.erase(pos);
						break;
					}
				}
			}
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			SDL_FPoint mousepos;
			SDL_RenderCoordinatesFromWindow(renderer, e->button.x, e->button.y, &mousepos.x, &mousepos.y);
			for (const auto &[k, v] : mcu->config->keymap) {
				SDL_FRect frect;
				SDL_RectToFRect(&v.rect, &frect);
				if (SDL_PointInRectFloat(&mousepos, &frect)) {
					mouse_held = true;
					held_button_mouse = k;
				}
			}
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			mouse_held = false;
			break;
	}

	const bool* keystates = SDL_GetKeyboardState(nullptr);
	for (int i = 0; i < SDL_SCANCODE_COUNT; ++i) {
		if (keystates[i] != 0) return;
	}
	if (SDL_GetGlobalMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) return;

	held_buttons.clear();
	enable_keypress = true;
}

void Keyboard::Render(SDL_Renderer *renderer) {
	SDL_Surface *tmp = SDL_CreateSurface(mcu->config->width, mcu->config->height, SDL_GetPixelFormatForMasks(32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000));
	for (const auto &k : held_buttons) SDL_FillSurfaceRect(tmp, &mcu->config->keymap[k].rect, 0xAA000000);
	if (mouse_held) SDL_FillSurfaceRect(tmp, &mcu->config->keymap[held_button_mouse].rect, 0xAA000000);

	SDL_Texture* tmp2 = SDL_CreateTextureFromSurface(renderer, tmp);
	SDL_DestroySurface(tmp);

	SDL_FRect dest {0, 0, static_cast<float>(mcu->config->width), static_cast<float>(mcu->config->height)};
	SDL_RenderTexture(renderer, tmp2, nullptr, &dest);

	SDL_DestroyTexture(tmp2);
}

unsigned int Keyboard::Tick() {
	uint8_t kimask = mcu->core.sfrs[0x42];
	// 16 KO lines. ES and old ES-plus drive them from the inverted 0x44/0x45
	// pair (no filter). Everything else drives from the 0x46/0x47 pair, gated
	// by the 0x44/0x45 KO filter (bit 1 = pin cut off from the KO register,
	// bit 0 = register-controlled; reset state 0 leaves all pins controlled).
	uint16_t ko;
	if (mcu->config->hardware_id == HW_ES || (mcu->config->hardware_id == HW_ES_PLUS && mcu->config->old_esp))
		ko = (mcu->core.sfrs[0x44] | mcu->core.sfrs[0x45] << 8) ^ 0xffff;
	else
		ko = (mcu->core.sfrs[0x46] | mcu->core.sfrs[0x47] << 8) & ~(mcu->core.sfrs[0x44] | mcu->core.sfrs[0x45] << 8);
	bool reset = false;

	struct Component { uint16_t ko_mask; uint8_t ki_mask; };
	Component comps[8];
	int ncomps = 0;

	auto add_key = [&](uint8_t k) {
		if (k == 0xff) {
			reset = true;
			return;
		}
		uint16_t ko_mask = 1 << (k >> 4);
		uint8_t ki_mask = 1 << (k & 0xf);
		int target = -1;
		for (int i = 0; i < ncomps; ++i) {
			if ((comps[i].ko_mask & ko_mask) || (comps[i].ki_mask & ki_mask)) {
				if (target < 0) {
					comps[i].ko_mask |= ko_mask;
					comps[i].ki_mask |= ki_mask;
					target = i;
				} else {
					comps[target].ko_mask |= comps[i].ko_mask;
					comps[target].ki_mask |= comps[i].ki_mask;
					comps[i--] = comps[--ncomps];
				}
			}
		}
		if (target < 0) comps[ncomps++] = Component{ko_mask, ki_mask};
	};

	for (const auto &k : held_buttons) add_key(k);
	if (mouse_held) add_key(held_button_mouse);

	if (reset) mcu->core.RequestReset();

	uint8_t ki = 0;
	for (int i = 0; i < ncomps; ++i) {
		if (comps[i].ko_mask & ko) ki |= comps[i].ki_mask;
	}

	// KI pins with pull-up disconnected (0x41 bit set) read idle and raise no
	// interrupts; their lines still conduct inside the matrix (ghost paths).
	ki &= ~mcu->core.sfrs[0x41];

	// EXICON (0F018H) bits 1:0 select the KI interrupt condition. KI pins
	// are active-low, so a falling pin edge is a KI bit going 0->1 (press).
	uint8_t pending;
	switch (mcu->core.sfrs[0x18] & 3) {
		case 0: pending = ki & ~prev_ki; break;  // falling edge (press)
		case 1: pending = ~ki & prev_ki; break;  // rising edge (release)
		case 2: pending = ~ki; break;            // level: pin high
		default: pending = ki; break;            // level: pin low
	}
	if (pending & kimask) mcu->core.interrupter->TryRaiseInterrupt(0, 1);
	prev_ki = ki;

	if (!reset) mcu->core.sfrs[0x40] = ki ^ 0xff;

	return 0;
}
