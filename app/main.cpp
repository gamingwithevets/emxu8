#include "mcu/mcu.h"
#include "peripheral/screen.h"
#include "peripheral/keyboard.h"
#include "peripheral/clockgen.h"
#include "gui/gui.h"
#include "gui/about.h"
#include "config/config.h"
#include "config/binary.h"
#include <cstdio>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <fstream>
#include <emxu8/emxu8.h>
#include <emxu8/compilerdefs.h>
#include <wx/wx.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

#if defined(_WIN32)
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

LONG WINAPI unhandled_filter(PEXCEPTION_POINTERS ep) {
	static bool handling = false;
	if (handling) return EXCEPTION_CONTINUE_SEARCH;
	handling = true;
	fprintf(stderr, "Big boner down the lane (%lx)\n", ep->ExceptionRecord->ExceptionRecord->ExceptionCode);
	print_traceback();
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
	return;
}
#endif

void sig_handler(int code) {
	static bool handling = false;
	if (handling) return;
	handling = true;
	fprintf(stderr, "Big boner down the lane (%x)\n", code);
	print_traceback();
	fprintf(stderr, "Trap raised, you can now debug this signal with a debugger of your choice.\n");
	debug_break();
}

class SDLPanel : public wxPanel {
public:
	SDLPanel(wxWindow* parent, MCU *mcu) : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS), mcu(mcu)
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

		img_interface = IMG_Load(mcu->config->interface_path.c_str());
		if (img_interface) {
			mcu->config->width = mcu->config->width ? mcu->config->width : img_interface->w;
			mcu->config->height = mcu->config->height ? mcu->config->height : img_interface->h;
		}

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
		if (img_interface) {
			SDL_DestroySurface(img_interface);
			img_interface = nullptr;
		}
	}

	void OnTimer(wxTimerEvent &) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			mcu->kb->ProcessEvent(renderer, &event);
			if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
				Close(true);
				running = false;
			}
		}

		if (!window || !renderer) return;
		SDL_FRect rect;

		SDL_SetRenderDrawColor(renderer, 0xD6, 0xE3, 0xD6, 0xFF);
		SDL_RenderFillRect(renderer, nullptr);

		SDL_Texture *img_interface_t = SDL_CreateTextureFromSurface(renderer, img_interface);
		SDL_RenderTexture(renderer, img_interface_t, nullptr, nullptr);

		mcu->screen->Present(renderer);
		mcu->kb->Render(renderer);

		SDL_RenderPresent(renderer);

		SDL_DestroyTexture(img_interface_t);
	}

	std::atomic<bool> running = true;
	SDL_Renderer *renderer = nullptr;
private:
	MCU *mcu = nullptr;
	SDL_Window *window = nullptr;
	SDL_Surface *img_interface = nullptr;
	wxTimer *timer = nullptr;
};

class MainFrame : public wxFrame {
	enum {
		ID_DEBUG_DEBUGGER = wxID_HIGHEST + 1,
		ID_DEBUG_HEXED,
		ID_DEBUG_BRKPOINT,
		ID_HARD_RESET,
	};
public:
	MainFrame(int argc, char *argv[]) : wxFrame(nullptr, wxID_ANY, "emX-U8") {
		if (argc < 2) {
			wxMessageBox("Please provide a path to a u8-emu-frontend-cpp configuration file!", "emX-U8");
			CallAfter([this] { Close(true); } );
			return;
		}

		SetWindowStyle(wxMINIMIZE_BOX | wxCLOSE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN);

		CreateMenuBar();
		Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
		Bind(wxEVT_MENU, &MainFrame::OnDebugger, this, ID_DEBUG_DEBUGGER);
		Bind(wxEVT_MENU, &MainFrame::OnHexEd, this, ID_DEBUG_HEXED);
		Bind(wxEVT_MENU, &MainFrame::OnBrkpoint, this, ID_DEBUG_BRKPOINT);
		Bind(wxEVT_MENU, &MainFrame::OnReset, this, wxID_RESET);
		Bind(wxEVT_MENU, &MainFrame::OnHardReset, this, ID_HARD_RESET);
		Bind(wxEVT_MENU, &MainFrame::OnCopyClip, this, wxID_COPY);
		Bind(wxEVT_MENU, &MainFrame::OnSavePNG, this, wxID_SAVEAS);
		Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnExit, this);

		std::ifstream is(argv[1], std::ifstream::binary);
		static Config config{};
		Binary::Read(is, config);

		if (!(config.hardware_id == HW_ES_PLUS && !config.old_esp)) {
			wxMessageBox("This version of emX-U8 only supports ES PLUS (later versions). Sorry!", "emX-U8");
			CallAfter([this] { Close(true); } );
			return;
		}

		SetClientSize({config.width, config.height});

		rom = new uint8_t[0x100000];
		memset(rom, 0xff, 0x100000);
		FILE *f = fopen(config.rom_file.c_str(), "rb");
		if (f) {
			fread(rom, sizeof(uint8_t), 0x100000, f);
			fclose(f);
		} else fprintf(stderr, "Failed to open ROM file: %s\n", config.rom_file.c_str());

		signal(SIGABRT, sig_handler);
		signal(SIGSEGV, sig_handler);
		signal(SIGFPE, sig_handler);
		mcu = new MCU(&config, rom, 0x8000, 0x8000, 0xe00);

		sdl_panel = new SDLPanel(this, mcu);
	}

	void OnDebuggerBtnRun(wxCommandEvent &) {
		if (single_step) SetSingleStep(false);
	}

	void OnDebuggerBtnStepInto(wxCommandEvent &) {
		if (single_step) {
			do { mcu->Tick(); } while (mcu->core.executor->task_running);
			debugger->Sync();
		}
	}

	void OnDebuggerBtnStepOver(wxCommandEvent &) {
		if (single_step) {
			uint32_t xpc = mcu->core.executor->cur_pc;
			if (xpc <= 0xfffff) {
				uint16_t opcode = mcu->core.ReadCodeMemory(xpc & 0xffff, xpc >> 16);
				auto ins = emxu8::U8Decoder::DecodeOpcode(opcode);
				if (ins.operand == emxu8::U8Decoder::OP_BL) {
					step_over_mode = true;
					step_over_brkpoint = xpc + (ins.arg1.argtype == emxu8::U8Decoder::ARG_REG ? 2 : 4);
					SetSingleStep(false);
					return;
				}
			}
			do { mcu->Tick(); } while (mcu->core.executor->task_running);
			debugger->Sync();
		}
	}

	void Run() {
		if (cpu_thread) {
			if (cpu_thread->joinable()) cpu_thread->join();
			delete cpu_thread;
		}
		cpu_thread = new std::jthread([&] {
			auto start = std::chrono::steady_clock::now();
			uint64_t tick_count = 0;
			bool first_run = true;
			bool brked = false;
			while (sdl_panel && sdl_panel->running && !single_step && !IsBeingDeleted()) {
				if (reset_mode) reset_mode = false;

				bool brk = false;
				uint32_t xpc = mcu->core.executor->cur_pc;

				if (!first_run) {
					if (!mcu->core.GetCodeRegion(mcu->core.pc, mcu->core.csr)) brk = true;
					else if (!brked && !mcu->core.interrupter->callstack.empty() && mcu->core.interrupter->callstack.back().intr.name == "BRK") {
						brk = true;
						brked = true;
					} else if (step_over_mode && step_over_brkpoint == xpc) {
						brk = true;
						step_over_mode = false;
					} else if (brkpoint)
						for (auto &panel : brkpoint->brkpoints) {
							if (panel->enabled && panel->type == BrkpointPanel::EXECUTE && (panel->seg << 16) + panel->addr == xpc) {
								brk = true;
								break;
							}
						}
				}

				if (brk) {
					CallAfter([this] {
						SetSingleStep(true);
						OpenDebugger();
					});
					break;
				}
				mcu->Tick();
				++tick_count;
				first_run = false;

				if (!IsBeingDeleted() && mcu->clock_gen->frequency > 0) {
					std::this_thread::sleep_until(start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
						std::chrono::duration<double>(tick_count / (double)mcu->clock_gen->frequency)));
				}
			}
			if (reset_mode && !single_step) CallAfter([this] { Run(); });
		});
	}

	void SetSingleStep(bool val) {
		single_step = val;
		if (debugger) debugger->SetStepButtons(val);
		if (val) {
			if (cpu_thread && cpu_thread->joinable()) cpu_thread->join();
			delete cpu_thread;
			cpu_thread = nullptr;
		} else Run();
	}

private:
	void CreateMenuBar() {
		auto *menubar = new wxMenuBar();

		auto *core_menu = new wxMenu();
		core_menu->Append(wxID_RESET, "&Soft Reset");
		core_menu->Append(ID_HARD_RESET, "&Hard Reset");
		menubar->Append(core_menu, "&Core");


		auto *video_save_display_menu = new wxMenu();
		video_save_display_menu->Append(wxID_COPY, "&Copy to clipboard");
		video_save_display_menu->Append(wxID_SAVEAS, "&Save as PNG...");

		auto *video_menu = new wxMenu();
		video_menu->AppendSubMenu(video_save_display_menu, "&Save display");
		menubar->Append(video_menu, "&Video");

		auto *debug_menu = new wxMenu();
		debug_menu->Append(ID_DEBUG_DEBUGGER, "&Debugger");
		debug_menu->Append(ID_DEBUG_HEXED, "&Hex editor");
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
			debugger->m_btn_stepover->Bind(wxEVT_BUTTON, &MainFrame::OnDebuggerBtnStepOver, this, debugger->m_btn_stepover->GetId());
		}
		debugger->Open();
	}

	void OnHexEd(wxCommandEvent &) {
		if (!hexed) hexed = new HexEditorDialog(mcu);
		hexed->Open();
	}

	void OnBrkpoint(wxCommandEvent &) {
		if (!brkpoint) brkpoint = new Brkpoint();
		brkpoint->Open();
	}

	void OnReset(wxCommandEvent &) {
		reset_mode = true;
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
		memset(mcu->core.sfrs, 0, 0x1000);
		mcu->Reset();
		if (!old_ss) SetSingleStep(false);
		else if (debugger) {
			debugger->Sync();
			debugger->SetStepButtons(true);
		}
	}

	void OnCopyClip(wxCommandEvent &) {
		bool ok = mcu->screen->CopyScreenToClipboard();
		if (ok) wxMessageBox("Display copied to clipboard successfully!", "Copy to clipboard");
		else wxMessageBox("An error occurred.", "Copy to clipboard", wxICON_ERROR);
	}

	void OnSavePNG(wxCommandEvent &) {
		wxFileDialog fd(this,
			"Save as PNG",
			"",
			"display.png",
			"PNG Files (*.png)|*.png",
			wxFD_SAVE | wxFD_OVERWRITE_PROMPT | wxFD_CHANGE_DIR
		);

		if (fd.ShowModal() != wxID_OK) return;
		if (mcu->screen->SaveScreenshotToPNG(fd.GetPath().data())) wxMessageBox("Display saved successfully!", "Save as PNG");
		else wxMessageBox("An error occurred.", "Save as PNG", wxICON_ERROR);
	}

	void OnExit(wxCloseEvent &event) {
		if (sdl_panel) sdl_panel->running = false;
		if (cpu_thread) {
			if (cpu_thread->joinable()) cpu_thread->join();
			delete cpu_thread;
			cpu_thread = nullptr;
		}
		if (debugger) debugger->Destroy();
		if (hexed) hexed->Destroy();
		if (brkpoint) brkpoint->Destroy();
		if (sdl_panel) sdl_panel->Destroy();
		event.Skip();
	}

	SDLPanel *sdl_panel = nullptr;
	MCU *mcu = nullptr;
	uint8_t *rom = nullptr;
	std::jthread *cpu_thread = nullptr;

	Debugger *debugger = nullptr;
	HexEditorDialog *hexed = nullptr;
	Brkpoint *brkpoint = nullptr;
	std::atomic<bool> single_step = false;

	bool step_over_mode = false;
	uint32_t step_over_brkpoint;

	bool reset_mode = false;
};

class App : public wxApp {
public:
	virtual ~App() = default;

	virtual bool OnInit() {
#ifdef _WIN32
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
		SetUnhandledExceptionFilter(unhandled_filter);
#ifdef _MSC_VER
		_set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
#endif
		std::signal(SIGABRT, sig_handler);
		std::signal(SIGSEGV, sig_handler);
		std::signal(SIGFPE, sig_handler);
		std::signal(SIGILL, sig_handler);
		auto *frame = new MainFrame(argc, argv);
		frame->Show(true);
		frame->SetSingleStep(false);
		return true;
	}
};

wxIMPLEMENT_APP(App);
