#include "keyboard.h"

Keyboard::Keyboard() {
	core->RegisterSFR(0x40, 1, emxu8::U8Core::DefaultRead<0xff>, emxu8::U8Core::NoWrite);
	core->RegisterSFR(0x41, 2, emxu8::U8Core::DefaultRead<0xff>, emxu8::U8Core::DefaultWrite<0xff>);
	core->RegisterSFR(0x44, 3, emxu8::U8Core::DefaultRead<0xff>, emxu8::U8Core::DefaultWrite<0xff>);
	core->RegisterSFR(0x47, 1, emxu8::U8Core::DefaultRead<0x83>, emxu8::U8Core::DefaultWrite<0x83>);
	held_buttons.clear();
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
	uint8_t ko = (mcu->config->hardware_id == HW_ES || (mcu->config->hardware_id == HW_ES_PLUS && mcu->config->old_esp)) ? (mcu->core.sfrs[0x44] ^ 0xff) : mcu->core.sfrs[0x46];
	bool reset = false;

	struct Component { uint8_t ko_mask, ki_mask; };
	Component comps[8];
	int ncomps = 0;

	auto add_key = [&](uint8_t k) {
		if (k == 0xff) {
			reset = true;
			return;
		}
		uint8_t ko_mask = 1 << (k >> 4);
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

	// TODO: use EXICON to determine how KI should be interpreted
	for (const auto &k : held_buttons) add_key(k);
	if (mouse_held) add_key(held_button_mouse);

	if (reset) mcu->core.RequestReset();

	uint8_t ki = 0;
	for (int i = 0; i < ncomps; ++i) {
		if (comps[i].ko_mask & ko) ki |= comps[i].ki_mask;
	}
	if (ki & kimask) mcu->core.interrupter->TryRaiseInterrupt(0, 1);

	if (!reset) mcu->core.sfrs[0x40] = ki ^ 0xff;

	return 0;
}
