#pragma once
#include "../config/config.h"
#include <SDL3/SDL.h>
#include <emxu8/emxu8.h>

struct statusbit {
	uint8_t idx;
	uint8_t bit;
};

class Screen : public emxu8::U8Peripheral {
	float status_bar_w = 0, status_bar_h = 0;
	SDL_Texture *img_status_bar_t = nullptr;

public:
	Config *config = nullptr;

	uint8_t width = 0;
	uint8_t height = 0;
	uint8_t row_bytes = 0;
	uint8_t stride_bytes = 0;
	SDL_Surface *screen_s = nullptr;
	std::vector<statusbit> status_bar_bits;

	explicit Screen(emxu8::U8Core *core, Config *config);
	void Reset() override;
	void InitStatusBarTexture(SDL_Renderer *renderer);
	void Present(SDL_Renderer *renderer);
};


