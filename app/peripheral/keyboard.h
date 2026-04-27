#pragma once
#include <cstdint>
#include <optional>
#include <SDL3/SDL.h>
#include <vector>
#include "peripheral.h"

enum es_stop_type : uint8_t {
	ES_STOP_GETKEY = 1,
	ES_STOP_ACBREAK = 2,
	ES_STOP_DOOFF = 3,
	ES_STOP_DDOUT = 4,
	ES_STOP_QRCODE_IN = 5,
	ES_STOP_QRCODE_OUT = 6,
	ES_STOP_QRCODE_IN3 = 7,
	ES_STOP_ACBREAK2 = 8
};

struct es_stop_info {
	es_stop_type *ES_STOPTYPEADR;
	uint8_t *ES_KIADR;
	uint8_t *ES_KOADR;
	char *ES_QR_DATATOP_ADR;
	bool qr_active;
	char qr_url[200];
};


class Keyboard : public Peripheral {
	std::vector<uint8_t> held_buttons;
	bool mouse_held;
	uint8_t held_button_mouse;
public:
	es_stop_info emu_kb;
	bool enable_keypress;

	Keyboard();
	void ProcessEvent(SDL_Renderer *renderer, const SDL_Event *e);
	void Render(SDL_Renderer *renderer);
	unsigned int Tick() override;
	std::optional<uint8_t> GetButton();
private:
	void TickRe(bool *reset, uint8_t *ki, uint8_t kimask, uint8_t ko, uint8_t k);
};
