#include "keyboard.h"

Keyboard::Keyboard() {
	core->RegisterSFR(0x40, 1, emxu8::U8Core::DefaultRead<0xff>, emxu8::U8Core::NoWrite);
	core->RegisterSFR(0x41, 2, emxu8::U8Core::DefaultRead<0xff>, emxu8::U8Core::DefaultWrite<0xff>);
	core->RegisterSFR(0x44, 3, emxu8::U8Core::DefaultRead<0xff>, emxu8::U8Core::DefaultWrite<0xff>);
	core->RegisterSFR(0x47, 1, emxu8::U8Core::DefaultRead<0x83>, emxu8::U8Core::DefaultWrite<0x83>);
}

void Keyboard::ProcessEvent(SDL_Renderer *renderer, const SDL_Event *e) {
	SDL_Keycode key;
	switch (e->type) {
		case SDL_EVENT_KEY_DOWN:
			key = e->key.key;
			for (const auto &[k, v] : mcu->config->keymap) {
				if (std::find(v.keys.begin(), v.keys.end(), key) != v.keys.end()) {
					held_buttons.push_back(k);
					break;
				}
			}
			break;
		case SDL_EVENT_KEY_UP:
			key = e->key.key;
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
	uint8_t ki = 0;
	uint8_t kimask = mcu->core.sfrs[0x42];
	uint8_t ko = (mcu->config->hardware_id == HW_ES || (mcu->config->hardware_id == HW_ES_PLUS && mcu->config->old_esp)) ? (mcu->core.sfrs[0x44] ^ 0xff) : mcu->core.sfrs[0x46];
	bool reset = false;

	// TODO: use EXICON to determine how KI should be interpreted
	for (const auto &k : held_buttons) TickRe(&reset, &ki, kimask, ko, k);
	if (mouse_held) TickRe(&reset, &ki, kimask, ko, held_button_mouse);

	if (!reset) mcu->core.sfrs[0x40] = ki ^ 0xff;

	return 0;
}

void Keyboard::TickRe(bool *reset, uint8_t *ki, uint8_t kimask, uint8_t ko, uint8_t k) {
	if (k != 0xff) {
		uint8_t ki_bit = k & 0xf;
		uint8_t ko_bit = k >> 4;
		if (ko & (1 << ko_bit)) {
			*ki |= (1 << ki_bit);
			if (kimask & (1 << ki_bit)) mcu->core.interrupter->TryRaiseInterrupt(0, 1);
		}
	} else {
		mcu->core.RequestReset();
		*reset = true;
	}
}
