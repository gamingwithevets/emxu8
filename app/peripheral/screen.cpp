#include "screen.h"
#include <cstdio>
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

	//                                                    ES/ES+ |5800P | FC
	status_bar_bits.push_back({0x0, 4}); // [S]
	status_bar_bits.push_back({0x0, 2}); // [A]
	status_bar_bits.push_back({0x1, 4}); // M
	status_bar_bits.push_back({0x1, 1}); // STO
	status_bar_bits.push_back({0x2, 6}); // RCL
	status_bar_bits.push_back({0x3, 6}); // STAT   | SD
	status_bar_bits.push_back({0x4, 7}); // CMPLX  | REG  | 360
	status_bar_bits.push_back({0x5, 6}); // MAT    | FMLA | SI
	if (config->hardware_id == HW_ES && config->is_5800p)
		status_bar_bits.push_back({0x5, 4}); //    | PRGM
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
	status_bar_w = img_status_bar->w;
	status_bar_h = img_status_bar->h;

	screen_s = SDL_CreateSurface(width * 3, height * 3, SDL_PIXELFORMAT_ARGB8888);
	SDL_FillSurfaceRect(screen_s, nullptr, NO_COLOR);

	for (uint16_t addr = 0x800; addr < 0xa00; addr += stride_bytes) core->RegisterSFR(addr, row_bytes, emxu8::U8Core::DefaultRead<0xff>, ScreenSFRWrite);

	core->RegisterSFR(0x30, 1, emxu8::U8Core::DefaultRead<7>, emxu8::U8Core::DefaultWrite<7>);
	core->RegisterSFR(0x31, 1, emxu8::U8Core::DefaultRead<7>, emxu8::U8Core::DefaultWrite<7>);
	core->RegisterSFR(0x32, 1, emxu8::U8Core::DefaultRead<0x1f>, emxu8::U8Core::DefaultWrite<0x1f>);
	core->RegisterSFR(0x33, 1, emxu8::U8Core::DefaultRead<7>, emxu8::U8Core::DefaultWrite<7>);
	core->RegisterSFR(0x34, 1, emxu8::U8Core::DefaultRead<3>, emxu8::U8Core::DefaultWrite<3>);
}

void Screen::Reset() {
	SDL_FillSurfaceRect(screen_s, nullptr, NO_COLOR);
	for (uint16_t addr = 0x800; addr < 0xa00; addr += stride_bytes) memset(&core->sfrs[addr], 0, row_bytes);
	memset(&core->sfrs[0x30], 0, 5);
}

SDL_Surface *Screen::GetCombinedSurface(uint32_t bg = NO_COLOR) {
	if (!config) return nullptr;
	int content_w = static_cast<int>(96 * config->pix_h);
	int content_h = static_cast<int>(status_bar_h + 31 * config->pix_h);
	auto *combined_s = SDL_CreateSurface(content_w, content_h, SDL_PIXELFORMAT_RGBA32);
	if (!combined_s) return nullptr;
	if (core->sfrs[0x31] != 5 && core->sfrs[0x31] != 6) return combined_s;
	SDL_FillSurfaceRect(combined_s, nullptr, bg);
	if (img_status_bar)
		for (int i = 0; i < status_bar_bits.size(); i++)
			if (core->sfrs[0x800+status_bar_bits[i].idx] & 1 << status_bar_bits[i].bit) {
				SDL_Rect dst = config->status_bar_crops[i];
				SDL_FillSurfaceRect(combined_s, &dst, BLACK_COLOR);
				SDL_BlitSurface(img_status_bar, &config->status_bar_crops[i], combined_s, &dst);
			}
	if (core->sfrs[0x31] == 6) return combined_s;
	SDL_Rect src = {0, config->pix_h, 96*config->pix_w, 31*config->pix_h};
	SDL_Rect dst = {0, status_bar_h, 96*config->pix_h, 31*config->pix_h};
	SDL_BlitSurfaceScaled(screen_s, &src, combined_s, &dst, SDL_SCALEMODE_NEAREST);
	return combined_s;
}

void Screen::Present(SDL_Renderer *renderer) {
	if (!config) return;
	auto *combined_s = GetCombinedSurface();
	if (!combined_s) return;
	auto *combined_t = SDL_CreateTextureFromSurface(renderer, combined_s);
	SDL_FRect rect = {
		static_cast<float>(config->screen_tl_w),
		static_cast<float>(config->screen_tl_h),
		static_cast<float>(combined_s->w),
		static_cast<float>(combined_s->h)
	};
	SDL_RenderTexture(renderer, combined_t, nullptr, &rect);
	SDL_DestroyTexture(combined_t);
	SDL_DestroySurface(combined_s);
}

struct ClipboardImageData {
	void *bmp_data;
	size_t bmp_size;
	void *png_data;
	size_t png_size;
};

static const void *ClipboardDataCallback(void *userdata, const char *mime_type, size_t *size) {
	auto *img = static_cast<ClipboardImageData *>(userdata);
	if (SDL_strcmp(mime_type, "image/bmp") == 0) {
		*size = img->bmp_size;
		return img->bmp_data;
	}
	*size = img->png_size;
	return img->png_data;
}

static void ClipboardCleanupCallback(void *userdata) {
	auto *img = static_cast<ClipboardImageData *>(userdata);
	SDL_free(img->bmp_data);
	SDL_free(img->png_data);
	delete img;
}

static void *ReadDynamicIOStream(SDL_IOStream *stream, size_t *out_size) {
	Sint64 size = SDL_GetIOSize(stream);
	if (size <= 0) {
		SDL_CloseIO(stream);
		*out_size = 0;
		return nullptr;
	}
	SDL_PropertiesID props = SDL_GetIOProperties(stream);
	void *ptr = SDL_GetPointerProperty(props, SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, nullptr);
	if (!ptr) {
		SDL_CloseIO(stream);
		*out_size = 0;
		return nullptr;
	}
	auto *copy = SDL_malloc(static_cast<size_t>(size));
	SDL_memcpy(copy, ptr, static_cast<size_t>(size));
	SDL_CloseIO(stream);
	*out_size = static_cast<size_t>(size);
	return copy;
}

bool Screen::CopyScreenToClipboard() {
	auto *combined_s = GetCombinedSurface(WHITE_COLOR_CASIO);
	if (!combined_s) return false;
	auto *bmp_stream = SDL_IOFromDynamicMem();
	bool bmp_ok = SDL_SaveBMP_IO(combined_s, bmp_stream, false);
	auto *png_stream = SDL_IOFromDynamicMem();
	bool png_ok = IMG_SavePNG_IO(combined_s, png_stream, false);
	SDL_DestroySurface(combined_s);
	auto *img = new ClipboardImageData{};
	if (bmp_ok)
		img->bmp_data = ReadDynamicIOStream(bmp_stream, &img->bmp_size);
	else
		SDL_CloseIO(bmp_stream);
	if (png_ok)
		img->png_data = ReadDynamicIOStream(png_stream, &img->png_size);
	else
		SDL_CloseIO(png_stream);
	if (!img->bmp_data && !img->png_data) {
		delete img;
		return false;
	}
	static const char *mime_types[] = {"image/bmp", "image/png", "public.png", "PNG"};
	SDL_SetClipboardData(ClipboardDataCallback, ClipboardCleanupCallback, img, mime_types, 4);
	return true;
}

bool Screen::SaveScreenshotToPNG(const char *path) {
	auto *combined_s = GetCombinedSurface(WHITE_COLOR_CASIO);
	if (!combined_s) return false;
	bool ok = IMG_SavePNG(combined_s, path);
	SDL_DestroySurface(combined_s);
	return ok;
}
