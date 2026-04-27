#pragma once
#include <wx/wx.h>
#include <cstdint>
#include <mutex>

class HexView : public wxScrolledWindow {
	static constexpr int ADDR_X   = 5;
	static constexpr int HEX_X    = 90;
	static constexpr int HEX_W    = 30;
	static constexpr int ASCII_X  = HEX_X + 16 * HEX_W + 20;
	static constexpr int ASCII_W  = 12;
public:
	HexView(wxWindow* parent);
	virtual ~HexView();

	void SetBuffer(uint8_t* buf, size_t size, uint32_t baseAddr);

private:
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

	size_t HitTest(int x, int y) const;
	void EditNibble(int nibble);
	wxRect ByteRect(size_t index) const;
	void EnsureCaretVisible();
	void CancelByteEdit();

	uint8_t* buffer = nullptr;
	size_t   bufferSize = 0;
	uint32_t baseAddress = 0;

	size_t caret = 0;
	size_t anchor = 0;
	size_t editIndex = 0;
	bool editingByte = false;
	uint8_t tempByte = 0;
	bool dragging = false;

	int rowHeight = 18;

	wxTimer refreshTimer;

	std::mutex bufferMutex;
};
