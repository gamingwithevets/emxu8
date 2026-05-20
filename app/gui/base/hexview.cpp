#include "hexview.h"

#include <wx/clipbrd.h>
#include <wx/dcbuffer.h>
#include <wx/tokenzr.h>
#include <algorithm>
#include <cctype>

HexView::HexView(wxWindow* parent, MCU *mcu)
	: wxScrolledWindow(parent), mcu(mcu)
{
	SetBackgroundStyle(wxBG_STYLE_PAINT);
	SetWindowStyleFlag(GetWindowStyleFlag() | wxWANTS_CHARS);

	SetScrollRate(0, rowHeight);

	// Keyboard handling
	Bind(wxEVT_CHAR_HOOK, &HexView::OnChar, this);
	Bind(wxEVT_KEY_DOWN,  &HexView::OnKeyDown, this);

	// Mouse
	Bind(wxEVT_LEFT_DOWN, &HexView::OnLeftDown, this);
	Bind(wxEVT_MOTION, &HexView::OnMouseMove, this);
	Bind(wxEVT_LEFT_UP, &HexView::OnLeftUp, this);

	// Painting
	Bind(wxEVT_PAINT, &HexView::OnPaint, this);
	Bind(wxEVT_ERASE_BACKGROUND, &HexView::OnEraseBackground, this);

	// Clipboard shortcuts
	wxAcceleratorEntry accels[] = {
		{ wxACCEL_CTRL, (int)'C', wxID_COPY  },
		{ wxACCEL_CTRL, (int)'V', wxID_PASTE }
	};
	SetAcceleratorTable(wxAcceleratorTable(2, accels));

	Bind(wxEVT_MENU, &HexView::OnCopy,  this, wxID_COPY);
	Bind(wxEVT_MENU, &HexView::OnPaste, this, wxID_PASTE);

	refreshTimer.SetOwner(this);
	Bind(wxEVT_TIMER, &HexView::OnRefreshTimer, this);
	refreshTimer.Start(50); // 20 FPS — smooth but cheap
}

HexView::~HexView() = default;

void HexView::SetBuffer(uint8_t* buf, size_t size, uint32_t addr) {
	buffer = buf;
	bufferSize = size;
	baseAddress = addr;

	caret = anchor = editIndex = 0;

	SetVirtualSize(0, ((size + 15) / 16) * rowHeight);
	Scroll(0, 0);

	SetFocus();
	Refresh(false);
}

void HexView::SetBuffer(readtype _read, writetype _write, size_t size, uint32_t addr) {
	buffer = reinterpret_cast<uint8_t *>(-1);
	read = _read;
	write = _write;
	bufferSize = size;
	baseAddress = addr;

	caret = anchor = editIndex = 0;

	SetVirtualSize(0, ((size + 15) / 16) * rowHeight);
	Scroll(0, 0);

	SetFocus();
	Refresh(false);
}

void HexView::OnEraseBackground(wxEraseEvent&) {
}

void HexView::OnPaint(wxPaintEvent&) {
	wxAutoBufferedPaintDC dc(this);

	dc.SetBackground(wxBrush(wxColour(255, 255, 255)));
	dc.Clear();

	wxFont font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
	dc.SetFont(font);

	int headerY = 0;

	dc.SetTextForeground(wxColour(128, 128, 128));
	dc.DrawText( wxString("Address"), 5, headerY);
	for (int col = 0; col < 16; col++) {
		int x = HEX_X + col * HEX_W;
		dc.DrawText(wxString::Format("%02X", col), x, headerY);
	}
	dc.DrawText(wxString("Decoded text"), ASCII_X, headerY);

	if (!buffer) return;
	PrepareDC(dc);

	int width, height;
	GetClientSize(&width, &height);

	int startRow = GetViewStart().y;
	int rowsVisible = height / rowHeight + 1;

	std::lock_guard lock(bufferMutex);

	for (int row = startRow; row < startRow + rowsVisible; row++) {
		size_t base = row * 16;
		if (base >= bufferSize) break;

		int y = (row + 1) * rowHeight;

		// Address
		dc.SetTextForeground(wxColour(0, 0, 255));
		dc.DrawText(
			wxString::Format("%06X ", baseAddress + (uint32_t)base),
			5, y
		);

		// Hex bytes
		for (int col = 0; col < 16; col++) {
			size_t idx = base + col;
			if (idx >= bufferSize) break;

			int x = HEX_X + col * HEX_W;

			bool selected =
				idx >= std::min(caret, anchor) &&
				idx <= std::max(caret, anchor);

			if (selected) {
				if (editMode == EditMode::Hex) dc.SetBrush(wxBrush(wxColour(0, 0x78, 0xd4)));
				else dc.SetBrush(wxBrush(wxColour(0xc2, 0xde, 0xf3)));
				dc.SetPen(*wxTRANSPARENT_PEN);
				dc.DrawRectangle(x - 2, y, 28, rowHeight);
			}

			if (idx == caret && editMode == EditMode::Hex) {
				dc.SetPen(wxPen(wxColour(0, 0, 0)));
				dc.SetBrush(*wxTRANSPARENT_BRUSH);
				dc.DrawRectangle(x - 3, y - 1, 30, rowHeight + 2);
			}

			if (editingByte && idx == editIndex) {
				dc.SetTextForeground(wxColour(255, 0, 0));
				dc.DrawText(wxString::Format("%X_", tempByte >> 4), x, y);
			} else {
				if (selected && editMode == EditMode::Hex) dc.SetTextForeground(wxColour(255, 255, 255));
				else dc.SetTextForeground(wxColour(0, 0, 0));
				dc.DrawText(wxString::Format("%02X", ReadBuffer(idx)), x, y);
			}
		}

		for (int col = 0; col < 16; col++) {
			size_t idx = base + col;
			if (idx >= bufferSize) break;

			int x = ASCII_X + col * ASCII_W;

			bool selected =
				idx >= std::min(caret, anchor) &&
				idx <= std::max(caret, anchor);

			if (selected) {
				if (editMode == EditMode::Ascii) dc.SetBrush(wxBrush(wxColour(0, 0x78, 0xd4)));
				else dc.SetBrush(wxBrush(wxColour(0xc2, 0xde, 0xf3)));
				dc.SetPen(*wxTRANSPARENT_PEN);
				dc.DrawRectangle(x - 1, y, ASCII_W, rowHeight);
			}

			if (idx == caret && editMode == EditMode::Ascii) {
				dc.SetPen(wxPen(wxColour(0, 0, 0)));
				dc.SetBrush(*wxTRANSPARENT_BRUSH);
				dc.DrawRectangle(x - 2, y - 1, ASCII_W + 2, rowHeight + 2);
			}

			unsigned char c = ReadBuffer(idx);
			wxChar ch = std::isprint(c) ? (wxChar)c : '.';

			if (selected && editMode == EditMode::Ascii) dc.SetTextForeground(wxColour(255, 255, 255));
			else dc.SetTextForeground(wxColour(0, 0, 0));
			dc.DrawText(wxString(ch), x, y);
		}
	}
}

void HexView::OnLeftDown(wxMouseEvent& e) {
	SetFocus();

	if (e.GetY() < rowHeight) return;

	int xx, yy;
	CalcUnscrolledPosition(e.GetX(), e.GetY() - rowHeight, &xx, &yy);

	int row = yy / rowHeight;
	if (row < 0) return;

	bool inHex   = xx >= HEX_X && xx < HEX_X + 16 * HEX_W;
	bool inAscii = xx >= ASCII_X && xx < ASCII_X + 16 * ASCII_W;
	int col;
	if (inHex) {
		col = (xx - HEX_X) / HEX_W;
		editMode = EditMode::Hex;
	}
	else if (inAscii) {
		col = (xx - ASCII_X) / ASCII_W;
		editMode = EditMode::Ascii;
	}
	else return;

	if (col < 0 || col >= 16) return;

	size_t idx = row * 16 + col;
	if (idx >= bufferSize) return;

	if (caret != idx) CancelByteEdit();
	caret = idx;
	if (!e.ShiftDown())
		anchor = idx;

	editIndex = caret;

	dragging = true;
	CaptureMouse();

	Refresh(false);
}

void HexView::OnMouseMove(wxMouseEvent& e) {
	if (!dragging || !e.LeftIsDown())
		return;
	if (e.GetY() < rowHeight) return;

	int xx, yy;
	CalcUnscrolledPosition(e.GetX(), e.GetY() - rowHeight, &xx, &yy);

	int row = yy / rowHeight;
	if (row < 0) return;
	int col = (xx - HEX_X) / HEX_W;

	if (col < 0 || col >= 16) return;

	size_t idx = row * 16 + col;
	if (idx >= bufferSize) return;

	if (idx != caret) {
		caret = idx;
		editIndex = caret;
		Refresh(false);
	}
}

void HexView::OnLeftUp(wxMouseEvent&) {
	if (dragging) {
		dragging = false;
		if (HasCapture())
			ReleaseMouse();
	}
}

void HexView::OnKeyDown(wxKeyEvent& e) {
	switch (e.GetKeyCode()) {
		case WXK_LEFT:
			if (caret > 0) caret--;
			break;
		case WXK_RIGHT:
			if (caret + 1 < bufferSize) caret++;
			break;
		case WXK_UP:
			if (caret >= 16) caret -= 16;
			break;
		case WXK_DOWN:
			if (caret + 16 < bufferSize) caret += 16;
			break;
		default:
			e.Skip();
			return;
	}

	if (!e.ShiftDown()) anchor = caret;

	editIndex = caret;

	EnsureCaretVisible();
	Refresh(false);

	e.StopPropagation();
}

void HexView::EnsureCaretVisible() {
	int viewX, viewY;
	GetViewStart(&viewX, &viewY);

	int clientW, clientH;
	GetClientSize(&clientW, &clientH);

	int rowsVisible = clientH / rowHeight;
	int caretRow = caret / 16;

	if (caretRow < viewY) {
		Scroll(viewX, caretRow);
	}
	else if (caretRow >= viewY + rowsVisible) {
		Scroll(viewX, caretRow - rowsVisible + 1);
	}
}

void HexView::OnChar(wxKeyEvent& e) {
	if (e.ControlDown() || e.AltDown()) {
		e.Skip();
		return;
	}

	int uc = e.GetUnicodeKey();
	if (uc == WXK_NONE || uc < 0)
		return;

	std::lock_guard lock(bufferMutex);

	// ASCII MODE
	if (editMode == EditMode::Ascii) {
		if (std::isprint((unsigned char)uc)) {
			WriteBuffer(caret, (uint8_t)uc);
			CancelByteEdit();

			if (caret + 1 < bufferSize)
				caret = anchor = editIndex = caret + 1;

			EnsureCaretVisible();
			Refresh(false);
		}
		return;
	}

	// HEX MODE
	if (!std::isxdigit((unsigned char)uc))
		return;

	int nibble = std::isdigit(uc)
		? uc - '0'
		: std::toupper(uc) - 'A' + 10;

	if (!editingByte) {
		tempByte = (uint8_t)(nibble << 4);
		editingByte = true;
	} else {
		tempByte |= (uint8_t)nibble;
		WriteBuffer(editIndex, tempByte);

		editingByte = false;

		if (editIndex + 1 < bufferSize)
			caret = anchor = editIndex = editIndex + 1;
	}

	EnsureCaretVisible();
	Refresh(false);
}

void HexView::OnCopy(wxCommandEvent&) {
	if (!buffer) return;

	size_t a = std::min(caret, anchor);
	size_t b = std::max(caret, anchor);

	wxString text;
	for (size_t i = a; i <= b; i++)
		text += wxString::Format("%02X ", ReadBuffer(i));

	if (wxTheClipboard->Open()) {
		wxTheClipboard->SetData(new wxTextDataObject(text));
		wxTheClipboard->Close();
	}
}

void HexView::OnPaste(wxCommandEvent&) {
	if (!buffer) return;
	if (!wxTheClipboard->Open()) return;

	wxTextDataObject data;
	if (!wxTheClipboard->GetData(data)) {
		wxTheClipboard->Close();
		return;
	}
	wxTheClipboard->Close();

	wxString text = data.GetText();

	std::lock_guard lock(bufferMutex);

	size_t idx = editIndex;
	bool high = true;
	uint8_t value = 0;

	for (wxChar c : text) {
		if (!std::isxdigit(c))
			continue;

		int nibble = std::isdigit(c)
			? c - '0'
			: std::toupper(c) - 'A' + 10;

		if (high) {
			value = nibble << 4;
			high = false;
		} else {
			value |= nibble;
			if (idx < bufferSize)
				WriteBuffer(idx++, value);
			high = true;
		}
	}

	caret = anchor = editIndex = idx;

	Refresh(false);
	EnsureCaretVisible();
}

void HexView::OnRefreshTimer(wxTimerEvent&) {
	if (!buffer)
		return;

	Refresh(false);
}

void HexView::CancelByteEdit() {
	editingByte = false;
	tempByte = 0;
}

uint8_t HexView::ReadBuffer(uint32_t addr) const {
	if (buffer && reinterpret_cast<uintptr_t>(buffer) != -1) return buffer[addr];
	return read(mcu, addr);
}

void HexView::WriteBuffer(uint32_t addr, uint8_t val) const {
	if (buffer && reinterpret_cast<uintptr_t>(buffer) != -1)  buffer[addr] = val;
	write(mcu, addr, val);
}
