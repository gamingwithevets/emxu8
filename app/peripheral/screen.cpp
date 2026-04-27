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

Screen::Screen(emxu8::U8Core *core, Config *_config) : U8Peripheral(core) {
	screen_ptr = this;
	config = _config;

	width = 96;
	height = 32;
	row_bytes = 12;
	stride_bytes = 16;

	status_bar_bits.push_back({0x0, 4}); // [S]
	status_bar_bits.push_back({0x0, 2}); // [A]
	status_bar_bits.push_back({0x1, 4}); // M
	status_bar_bits.push_back({0x1, 1}); // STO
	status_bar_bits.push_back({0x2, 6}); // RCL
	status_bar_bits.push_back({0x3, 6}); // STAT   | SD
	status_bar_bits.push_back({0x4, 7}); // CMPLX  | REG  | 360
	status_bar_bits.push_back({0x5, 6}); // MAT    | FMLA | SI
	if (config->hardware_id == HW_ES && config->is_5800p)
		status_bar_bits.push_back({0x5, 4}); //   | PRGM
	status_bar_bits.push_back({0x5, 1}); // VCT    | END  | DMY
	status_bar_bits.push_back({0x7, 5}); // [D]
	status_bar_bits.push_back({0x7, 1}); // [R]
	status_bar_bits.push_back({0x8, 4}); // [G]
	status_bar_bits.push_back({0x8, 0}); // FIX
	status_bar_bits.push_back({0x9, 5}); // SCI
	status_bar_bits.push_back({0xa, 6}); // Math
	status_bar_bits.push_back({0xa, 3}); // ▼
	status_bar_bits.push_back({0xb, 7}); // ▲
	status_bar_bits.push_back({0xb, 4}); // Disp

	img_status_bar = IMG_Load(config->status_bar_path.c_str());
	screen_s = SDL_CreateSurface(width * 3, height * 3, SDL_PIXELFORMAT_ARGB8888);
	SDL_FillSurfaceRect(screen_s, nullptr, NO_COLOR);

	for (uint16_t addr = 0x800; addr < 0xa00; addr += stride_bytes) core->RegisterSFR(addr, row_bytes, emxu8::U8Core::DefaultRead<0xff>, ScreenSFRWrite);
}

void Screen::Reset() {
	SDL_FillSurfaceRect(screen_s, nullptr, NO_COLOR);
}

void Screen::Present(SDL_Renderer *renderer) {
	if (!config) return;

	SDL_FRect rect;
	SDL_FRect rect2;
	SDL_Texture *img_status_bar_t = SDL_CreateTextureFromSurface(renderer, img_status_bar);
	for (int i = 0; i < status_bar_bits.size(); i++)
		if (core->sfrs[0x800+status_bar_bits[i].idx] & 1 << status_bar_bits[i].bit) {
			SDL_FRect srcfrect;
			rect = {
				static_cast<float>(config->screen_tl_w + config->status_bar_crops[i].x),
				static_cast<float>(config->screen_tl_h + config->status_bar_crops[i].y),
				static_cast<float>(config->status_bar_crops[i].w),
				static_cast<float>(config->status_bar_crops[i].h)
			};
			SDL_RectToFRect(&config->status_bar_crops[i], &srcfrect);
			SDL_RenderTexture(renderer, img_status_bar_t, &srcfrect, &rect);
		}

	auto *screen_t = SDL_CreateTextureFromSurface(renderer, screen_s);
	rect = {static_cast<float>(config->screen_tl_w), static_cast<float>(config->screen_tl_h + img_status_bar->h), 96*3, 31*3};
	rect2 = {0, static_cast<float>(config->pix_h), 96*static_cast<float>(config->pix_w), 31*static_cast<float>(config->pix_h)};
	SDL_RenderTexture(renderer, screen_t, &rect2, &rect);

	SDL_DestroyTexture(screen_t);
	SDL_DestroyTexture(img_status_bar_t);
}
