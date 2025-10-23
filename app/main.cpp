#include "mcu/mcu.h"
#include "peripheral/screen.h"
#include "gui/gui.h"
#include "gui/about.h"
#include <cstdio>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <emxu8/emxu8.h>
#include <emxu8/compilerdefs.h>
#include <wx/wx.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

void traceback_handler(int sig = 0) {
	fprintf(stderr, "The boner was so big that it could even trash the stack.");
	if (sig != 0) printf(" (%d)", sig);
	fprintf(stderr, "\n");
}

void print_traceback() {
	void* stack[62];
	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);
	WORD frames = CaptureStackBackTrace(0, 62, stack, NULL);

	SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	fprintf(stderr, "Stack trace:\n");
	for(int i = 0; i < frames; ++i) {
		SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
		fprintf(stderr, "  %2d [0x%llx]  %s\n", i, symbol->Address, symbol->Name);
	}

	free(symbol);
}

LONG WINAPI veh_handler(PEXCEPTION_POINTERS ep) {
	static bool handling = false;
	if (handling) return EXCEPTION_CONTINUE_SEARCH;
	handling = true;
	fprintf(stderr, "Big boner down the lane (0x%lx)\n", ep->ExceptionRecord->ExceptionCode);
	print_traceback();
	fprintf(stderr, "Trap raised, you can now debug this signal with a debugger of your choice.\n");
	std::signal(SIGILL, SIG_DFL);
	debug_break();
	return EXCEPTION_EXECUTE_HANDLER;
}
#elif defined(__unix__) || defined(__APPLE__)
#include <execinfo.h>
#include <cstdlib>

void print_traceback() {
	void* buffer[64];
	int nptrs = backtrace(buffer, 64);
	char** strings = backtrace_symbols(buffer, nptrs);
	fprintf(stderr, "Stack trace:\n");
	for(int i = 0; i < nptrs; ++i) fprintf(stderr, "  %2d  %s\n", i, strings[i]);
	free(strings);
}
#else
void print_traceback() {
	traceback_handler();
}
#endif

void sig_handler(int signum) {
	static bool handling = false;
	if (handling) return;
	handling = true;
	fprintf(stderr, "Big boner down the lane (%d)\n", signum);
	print_traceback();
	fprintf(stderr, "Trap raised, you can now debug this signal with a debugger of your choice.\n");
	debug_break();
}

class SDLPanel : public wxPanel {
public:
	SDLPanel(wxWindow* parent, Screen *screen) : wxPanel(parent, wxID_ANY), screen(screen)
	{
		SDL_PropertiesID props = SDL_CreateProperties();
#ifdef __WXMSW__
		SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, GetHandle());
#elif defined(__WXGTK__)
		SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER, GDK_WINDOW_XID(GetHandle()));
#elif defined(__WXOSX__)
		SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_WINDOW_POINTER, GetHandle());
#endif
		window = SDL_CreateWindowWithProperties(props);
		renderer = SDL_CreateRenderer(window, nullptr);

		img_interface = IMG_Load("images/interface_esp_991esp.png");

		Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
			wxSize size = event.GetSize();
			SDL_SetWindowSize(window, size.x, size.y);
			event.Skip();
		});

		timer = new wxTimer(this);
		Bind(wxEVT_TIMER, &SDLPanel::OnTimer, this);
		timer->Start(16); // ~60 FPS
	}

	~SDLPanel() override {
		timer->Stop();
		if (renderer) {
			SDL_DestroyRenderer(renderer);
			renderer = nullptr;
		}
		if (window) {
			SDL_DestroyWindow(window);
			window = nullptr;
		}
	}

	void OnTimer(wxTimerEvent &) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_EVENT_QUIT:
					Close(true);
					running = false;
					break;
			}
		}

		if (!window || !renderer) return;
		SDL_FRect rect;

		SDL_SetRenderDrawColor(renderer, 0xD6, 0xE3, 0xD6, 0xFF);
		SDL_RenderFillRect(renderer, nullptr);

		SDL_Texture *img_interface_t = SDL_CreateTextureFromSurface(renderer, img_interface);
		SDL_RenderTexture(renderer, img_interface_t, nullptr, nullptr);

		screen->Present(renderer);

		SDL_RenderPresent(renderer);

		SDL_DestroyTexture(img_interface_t);
	}

	std::atomic<bool> running = true;
private:
	Screen *screen = nullptr;
	SDL_Window *window = nullptr;
	SDL_Renderer *renderer = nullptr;
	SDL_Surface *img_interface = nullptr;
	wxTimer *timer = nullptr;
};

class MainFrame : public wxFrame {
	enum {
		ID_DEBUG_DEBUGGER = wxID_HIGHEST + 1,
		ID_DEBUG_BRKPOINT,
		ID_HARD_RESET,
	};
public:
	MainFrame() : wxFrame(nullptr, wxID_ANY, "emX-U8") {
		SetWindowStyle(wxMINIMIZE_BOX | wxCLOSE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN);
		SetClientSize({405, 816});

		CreateMenuBar();
		Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
		Bind(wxEVT_MENU, &MainFrame::OnDebugger, this, ID_DEBUG_DEBUGGER);
		Bind(wxEVT_MENU, &MainFrame::OnBrkpoint, this, ID_DEBUG_BRKPOINT);
		Bind(wxEVT_MENU, &MainFrame::OnReset, this, wxID_RESET);
		Bind(wxEVT_MENU, &MainFrame::OnHardReset, this, ID_HARD_RESET);
		Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnExit, this);

		uint8_t rom[0x100000];
		memset(rom, 0xff, sizeof(rom));
		FILE *f = fopen("roms/GY454XE .bin", "rb");
		fread(rom, sizeof(uint8_t), sizeof(rom), f);

		signal(SIGABRT, sig_handler);
		signal(SIGSEGV, sig_handler);
		signal(SIGFPE, sig_handler);
		mcu = new MCU(rom, 0x8000, 0x8000, 0xe00);

		sdl_panel = new SDLPanel(this, &mcu->screen);
	}

	void OnDebuggerBtnRun(wxCommandEvent &) {
		if (single_step) SetSingleStep(false);
	}

	void OnDebuggerBtnStepInto(wxCommandEvent &) {
		if (single_step) {
			mcu->Tick();
			debugger->Sync();
		}
	}

	void Run() {
		cpu_thread = new std::jthread([&] {
			auto start = std::chrono::steady_clock::now();
			while (sdl_panel && sdl_panel->running && !single_step && !IsBeingDeleted()) {
				bool brk = false;
				if (brkpoint)
					for (auto &panel : brkpoint->brkpoints)
						if (panel->enabled && panel->type == BrkpointPanel::EXECUTE && panel->seg == mcu->core.csr && panel->addr == mcu->core.pc) {
							brk = true;
							break;
						}

				if (brk) {
					CallAfter([this] {
						SetSingleStep(true);
						OpenDebugger();
					});
					break;
				}
				mcu->Tick();

				if (!IsBeingDeleted()) {
					auto now = std::chrono::steady_clock::now();
					double elapsed_s = std::chrono::duration<double>(now - start).count();
					auto target_cycles = static_cast<uint64_t>(elapsed_s * mcu->frequency);
					if (mcu->core.cycle_count > target_cycles) std::this_thread::sleep_for(std::chrono::microseconds(50LL));
				}
			}
		});
	}

	void SetSingleStep(bool val) {
		single_step = val;
		if (debugger) debugger->SetStepButtons(val);
		if (val) {
			if (cpu_thread && cpu_thread->joinable()) cpu_thread->join();
		} else Run();
	}

private:
	void CreateMenuBar() {
		auto *menubar = new wxMenuBar();

		auto *core_menu = new wxMenu();
		core_menu->Append(wxID_RESET, "&Soft Reset");
		core_menu->Append(ID_HARD_RESET, "&Hard Reset");
		menubar->Append(core_menu, "&Core");

		auto *debug_menu = new wxMenu();
		debug_menu->Append(ID_DEBUG_DEBUGGER, "&Debugger");
		debug_menu->Append(ID_DEBUG_BRKPOINT, "&Manage breakpoints");
		menubar->Append(debug_menu, "&Debug");

		auto *help_menu = new wxMenu();
		help_menu->Append(wxID_ABOUT, "&About emX-U8");
		menubar->Append(help_menu, "&Help");

		SetMenuBar(menubar);
	}

	void OnAbout(wxCommandEvent &) {
		AboutDialog dlg(this);
		dlg.Open();
	}

	void OnDebugger(wxCommandEvent &) { OpenDebugger(); }
	void OpenDebugger() {
		if (!debugger) {
			debugger = new Debugger(&mcu->core, single_step);
			debugger->m_btn_run->Bind(wxEVT_BUTTON, &MainFrame::OnDebuggerBtnRun, this, debugger->m_btn_run->GetId());
			debugger->m_btn_stepinto->Bind(wxEVT_BUTTON, &MainFrame::OnDebuggerBtnStepInto, this, debugger->m_btn_stepinto->GetId());
		}
		debugger->Open();
	}

	void OnBrkpoint(wxCommandEvent &) {
		if (!brkpoint) {
			brkpoint = new Brkpoint();
		}
		brkpoint->Open();
	}

	void OnReset(wxCommandEvent &) {
		bool old_ss = single_step;
		if (!old_ss) SetSingleStep(true);
		mcu->Reset();
		if (!old_ss) SetSingleStep(false);
		else if (debugger) {
			debugger->Sync();
			debugger->SetStepButtons(true);
		}
	}

	void OnHardReset(wxCommandEvent &) {
		bool old_ss = single_step;
		if (!old_ss) SetSingleStep(true);
		mcu->InitializeRAM();
		mcu->Reset();
		if (!old_ss) SetSingleStep(false);
		else if (debugger) {
			debugger->Sync();
			debugger->SetStepButtons(true);
		}
	}

	void OnExit(wxCloseEvent &event) {
		if (debugger) debugger->Destroy();
		if (brkpoint) brkpoint->Destroy();
		if (sdl_panel) sdl_panel->Destroy();
		event.Skip();
	}

	SDLPanel *sdl_panel = nullptr;
	MCU *mcu = nullptr;
	std::jthread *cpu_thread = nullptr;

	Debugger *debugger = nullptr;
	Brkpoint *brkpoint = nullptr;
	std::atomic<bool> single_step = false;
};

class App : public wxApp {
public:
	virtual ~App() = default;

	virtual bool OnInit() {
#ifdef _WIN32
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
		AddVectoredExceptionHandler(1, veh_handler);
#ifdef _MSC_VER
		_set_abort_behavior(0, _WRITE_ABOT_MSG);
#endif
#endif
		std::signal(SIGABRT, sig_handler);
		std::signal(SIGSEGV, sig_handler);
		std::signal(SIGFPE, sig_handler);
		std::signal(SIGILL, sig_handler);
		auto *frame = new MainFrame();
		frame->Show(true);
		frame->SetSingleStep(false);
		return true;
	}
};

wxIMPLEMENT_APP(App);
