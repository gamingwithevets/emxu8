#pragma once
#include <wx/wx.h>
#include <mutex>
#include "../../mcu/mcu.h"

typedef uint8_t (*readtype)(MCU *, uint16_t);
typedef void (*writetype)(MCU *, uint16_t, uint8_t);

namespace HexViewLayout {
	constexpr int ROW_HEIGHT = 18;
	constexpr int ADDR_X     = 5;
	constexpr int HEX_X      = 90;
	constexpr int HEX_W      = 30;
	constexpr int ASCII_X    = HEX_X + 16 * HEX_W + 20;
	constexpr int ASCII_W    = 12;
}

class HexHeader : public wxWindow {
public:
	explicit HexHeader(wxWindow* parent);

private:
	void OnPaint(wxPaintEvent&);
	void OnEraseBackground(wxEraseEvent&);
};

class HexGrid : public wxScrolledWindow {
public:
	HexGrid(wxWindow* parent, MCU *mcu);
	virtual ~HexGrid();

	void SetBuffer(uint8_t* buf, size_t size, uint32_t baseAddr);
	void SetBuffer(readtype _read, writetype _write, size_t size, uint32_t baseAddr);

private:
	MCU *mcu;
	enum class EditMode {
		Hex,
		Ascii
	};

	EditMode editMode = EditMode::Hex;

	void OnPaint(wxPaintEvent&);
	void OnLeftDown(wxMouseEvent&);
	void OnMouseMove(wxMouseEvent&);
	void OnLeftUp(wxMouseEvent&);
	void OnKeyDown(wxKeyEvent&);
	void OnChar(wxKeyEvent&);
	void OnCopy(wxCommandEvent&);
	void OnPaste(wxCommandEvent&);
	void OnEraseBackground(wxEraseEvent&);
	void OnRefreshTimer(wxTimerEvent&);

	void EnsureCaretVisible();
	void CancelByteEdit();
	uint8_t ReadBuffer(uint32_t addr);
	void WriteBuffer(uint32_t addr, uint8_t val);

	uint8_t* buffer = nullptr;
	size_t   bufferSize = 0;
	uint32_t baseAddress = 0;
	readtype read;
	writetype write;

	size_t caret = 0;
	size_t anchor = 0;
	size_t editIndex = 0;
	bool editingByte = false;
	uint8_t tempByte = 0;
	bool dragging = false;

	wxTimer refreshTimer;

	std::mutex bufferMutex;
};

class HexView : public wxPanel {
public:
	HexView(wxWindow* parent, MCU *mcu);

	void SetBuffer(uint8_t* buf, size_t size, uint32_t baseAddr);
	void SetBuffer(readtype _read, writetype _write, size_t size, uint32_t baseAddr);

private:
	HexHeader* header;
	HexGrid*   grid;
};
