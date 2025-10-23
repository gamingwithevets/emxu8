#pragma once
#include <SDL3/SDL.h>
#include <emxu8/emxu8.h>

class Screen : public emxu8::U8Peripheral {
	SDL_Surface *img_status_bar = nullptr;

public:
	uint8_t width = 0;
	uint8_t height = 0;
	uint8_t row_bytes = 0;
	uint8_t stride_bytes = 0;
	SDL_Surface *screen_s = nullptr;

	explicit Screen(emxu8::U8Core *core);
	void Reset() override;
	void Present(SDL_Renderer *renderer);
};


