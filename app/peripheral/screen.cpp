#include "screen.h"
#include <cstdlib>
#include <mutex>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <emxu8/emxu8.h>

Screen *screen_ptr;

const uint32_t NO_COLOR = 0;
const uint32_t WHITE_COLOR = 0xFFFFFFFF;
const uint32_t WHITE_COLOR_CASIO = 0xFFD6E3D6;
const uint32_t GRAY1_COLOR = 0xFFAAAAAA;
const uint32_t GRAY2_COLOR = 0xFF555555;
const uint32_t BLACK_COLOR = 0xFF000000;

void ScreenSFRWrite(emxu8::U8Core *core, uint16_t addr, uint8_t val) {
	core->sfrs[addr] = val;

	addr -= 0x800;
	int y = addr / screen_ptr->stride_bytes;
	int x = addr % screen_ptr->stride_bytes * 8;

	int j;
	SDL_Rect rect;
	for (int i = 7; i >= 0; i--) {
		j = ~i & 7;
		rect = {(x + j) * 3, y * 3, 3, 3};
		SDL_FillSurfaceRect(screen_ptr->screen_s, &rect, val & 1 << i ? BLACK_COLOR : NO_COLOR);
	}
}

Screen::Screen(emxu8::U8Core *core) : U8Peripheral(core) {
	screen_ptr = this;

	width = 96;
	height = 32;
	row_bytes = 12;
	stride_bytes = 16;

	img_status_bar = IMG_Load("images/interface_es_bar.png");
	screen_s = SDL_CreateSurface(width * 3, height * 3, SDL_PIXELFORMAT_ARGB8888);
	SDL_FillSurfaceRect(screen_s, nullptr, NO_COLOR);

	for (uint16_t addr = 0x800; addr < 0xa00; addr += stride_bytes) core->RegisterSFR(addr, row_bytes, emxu8::U8Core::DefaultRead<0xff>, ScreenSFRWrite);
}

void Screen::Reset() {
}

void Screen::Present(SDL_Renderer *renderer) {
	SDL_FRect rect;
	SDL_FRect rect2;
	SDL_Texture *img_status_bar_t = SDL_CreateTextureFromSurface(renderer, img_status_bar);
	rect = {58, 128, static_cast<float>(img_status_bar->w), static_cast<float>(img_status_bar->h)};
	SDL_RenderTexture(renderer, img_status_bar_t, nullptr, &rect);

	auto *screen_t = SDL_CreateTextureFromSurface(renderer, screen_s);
	rect = {58, 140, 96*3, 31*3};
	rect2 = {0, 3, 96*3, 31*3};
	SDL_RenderTexture(renderer, screen_t, &rect2, &rect);

	SDL_DestroyTexture(screen_t);
	SDL_DestroyTexture(img_status_bar_t);
}
