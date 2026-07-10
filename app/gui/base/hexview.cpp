#include "hexview.h"

#include <wx/clipbrd.h>
#include <wx/dcbuffer.h>
#include <wx/tokenzr.h>
#include <algorithm>
#include <cctype>

using namespace HexViewLayout;

HexHeader::HexHeader(wxWindow* parent)
	: wxWindow(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, ROW_HEIGHT))
{
	SetBackgroundStyle(wxBG_STYLE_PAINT);
	Bind(wxEVT_PAINT, &HexHeader::OnPaint, this);
	Bind(wxEVT_ERASE_BACKGROUND, &HexHeader::OnEraseBackground, this);
}

void HexHeader::OnEraseBackground(wxEraseEvent&) {
}

void HexHeader::OnPaint(wxPaintEvent&) {
	wxAutoBufferedPaintDC dc(this);

	dc.SetBackground(wxBrush(wxColour(255, 255, 255)));
	dc.Clear();

	wxFont font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
	dc.SetFont(font);

	dc.SetTextForeground(wxColour(128, 128, 128));
	dc.DrawText(wxString("Address"), ADDR_X, 0);
	for (int col = 0; col < 16; col++) {
		int x = HEX_X + col * HEX_W;
		dc.DrawText(wxString::Format("%02X", col), x, 0);
	}
	dc.DrawText(wxString("Decoded text"), ASCII_X, 0);
}

HexGrid::HexGrid(wxWindow* parent, MCU *mcu)
	: wxScrolledWindow(parent), mcu(mcu)
{
	SetBackgroundStyle(wxBG_STYLE_PAINT);
	SetWindowStyleFlag(GetWindowStyleFlag() | wxWANTS_CHARS);

	SetScrollRate(0, ROW_HEIGHT);

	Bind(wxEVT_CHAR_HOOK, &HexGrid::OnChar, this);
	Bind(wxEVT_KEY_DOWN,  &HexGrid::OnKeyDown, this);

	Bind(wxEVT_LEFT_DOWN, &HexGrid::OnLeftDown, this);
	Bind(wxEVT_MOTION, &HexGrid::OnMouseMove, this);
	Bind(wxEVT_LEFT_UP, &HexGrid::OnLeftUp, this);

	Bind(wxEVT_PAINT, &HexGrid::OnPaint, this);
	Bind(wxEVT_ERASE_BACKGROUND, &HexGrid::OnEraseBackground, this);

	wxAcceleratorEntry accels[] = {
		{ wxACCEL_CTRL, (int)'C', wxID_COPY  },
		{ wxACCEL_CTRL, (int)'V', wxID_PASTE }
	};
	SetAcceleratorTable(wxAcceleratorTable(2, accels));

	Bind(wxEVT_MENU, &HexGrid::OnCopy,  this, wxID_COPY);
	Bind(wxEVT_MENU, &HexGrid::OnPaste, this, wxID_PASTE);

	refreshTimer.SetOwner(this);
	Bind(wxEVT_TIMER, &HexGrid::OnRefreshTimer, this);
	refreshTimer.Start(50); // 20 FPS — smooth but cheap
}

HexGrid::~HexGrid() = default;

void HexGrid::SetBuffer(uint8_t* buf, size_t size, uint32_t addr) {
	buffer = buf;
	bufferSize = size;
	baseAddress = addr;

	caret = anchor = editIndex = 0;

	SetVirtualSize(0, ((size + 15) / 16) * ROW_HEIGHT);
	Scroll(0, 0);

	SetFocus();
	Refresh(false);
}

void HexGrid::SetBuffer(readtype _read, writetype _write, size_t size, uint32_t addr) {
	buffer = reinterpret_cast<uint8_t *>(-1);
	read = _read;
	write = _write;
	bufferSize = size;
	baseAddress = addr;

	caret = anchor = editIndex = 0;

	SetVirtualSize(0, ((size + 15) / 16) * ROW_HEIGHT);
	Scroll(0, 0);

	SetFocus();
	Refresh(false);
}

void HexGrid::OnEraseBackground(wxEraseEvent&) {
}

void HexGrid::OnPaint(wxPaintEvent&) {
	wxAutoBufferedPaintDC dc(this);

	dc.SetBackground(wxBrush(wxColour(255, 255, 255)));
	dc.Clear();

	if (!buffer) return;

	wxFont font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
	dc.SetFont(font);

	PrepareDC(dc);

	int width, height;
	GetClientSize(&width, &height);

	int startRow = GetViewStart().y;
	int rowsVisible = height / ROW_HEIGHT + 1;

	for (int row = startRow; row < startRow + rowsVisible; row++) {
		size_t base = row * 16;
		if (base >= bufferSize) break;

		int y = row * ROW_HEIGHT;

		dc.SetTextForeground(wxColour(0, 0, 255));
		dc.DrawText(
			wxString::Format("%06X ", baseAddress + (uint32_t)base),
			ADDR_X, y
		);

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
				dc.DrawRectangle(x - 2, y, 28, ROW_HEIGHT);
			}

			if (idx == caret && editMode == EditMode::Hex) {
				dc.SetPen(wxPen(wxColour(0, 0, 0)));
				dc.SetBrush(*wxTRANSPARENT_BRUSH);
				dc.DrawRectangle(x - 3, y - 1, 30, ROW_HEIGHT + 2);
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
				dc.DrawRectangle(x - 1, y, ASCII_W, ROW_HEIGHT);
			}

			if (idx == caret && editMode == EditMode::Ascii) {
				dc.SetPen(wxPen(wxColour(0, 0, 0)));
				dc.SetBrush(*wxTRANSPARENT_BRUSH);
				dc.DrawRectangle(x - 2, y - 1, ASCII_W + 2, ROW_HEIGHT + 2);
			}

			unsigned char c = ReadBuffer(idx);
			wxChar ch = std::isprint(c) ? (wxChar)c : '.';

			if (selected && editMode == EditMode::Ascii) dc.SetTextForeground(wxColour(255, 255, 255));
			else dc.SetTextForeground(wxColour(0, 0, 0));
			dc.DrawText(wxString(ch), x, y);
		}
	}
}

void HexGrid::OnLeftDown(wxMouseEvent& e) {
	SetFocus();

	int xx, yy;
	CalcUnscrolledPosition(e.GetX(), e.GetY(), &xx, &yy);

	int row = yy / ROW_HEIGHT;
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

void HexGrid::OnMouseMove(wxMouseEvent& e) {
	if (!dragging || !e.LeftIsDown())
		return;

	int xx, yy;
	CalcUnscrolledPosition(e.GetX(), e.GetY(), &xx, &yy);

	int row = yy / ROW_HEIGHT;
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

void HexGrid::OnLeftUp(wxMouseEvent&) {
	if (dragging) {
		dragging = false;
		if (HasCapture())
			ReleaseMouse();
	}
}

void HexGrid::OnKeyDown(wxKeyEvent& e) {
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

void HexGrid::EnsureCaretVisible() {
	int viewX, viewY;
	GetViewStart(&viewX, &viewY);

	int clientW, clientH;
	GetClientSize(&clientW, &clientH);

	int rowsVisible = clientH / ROW_HEIGHT;
	int caretRow = caret / 16;

	if (caretRow < viewY) {
		Scroll(viewX, caretRow);
	}
	else if (caretRow >= viewY + rowsVisible) {
		Scroll(viewX, caretRow - rowsVisible + 1);
	}
}

void HexGrid::OnChar(wxKeyEvent& e) {
	if (e.ControlDown() || e.AltDown()) {
		e.Skip();
		return;
	}

	int uc = e.GetUnicodeKey();
	if (uc == WXK_NONE || uc < 0)
		return;

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

void HexGrid::OnCopy(wxCommandEvent&) {
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

void HexGrid::OnPaste(wxCommandEvent&) {
	if (!buffer) return;
	if (!wxTheClipboard->Open()) return;

	wxTextDataObject data;
	if (!wxTheClipboard->GetData(data)) {
		wxTheClipboard->Close();
		return;
	}
	wxTheClipboard->Close();

	wxString text = data.GetText();

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

void HexGrid::OnRefreshTimer(wxTimerEvent&) {
	if (!buffer)
		return;

	Refresh(false);
}

void HexGrid::CancelByteEdit() {
	editingByte = false;
	tempByte = 0;
}

uint8_t HexGrid::ReadBuffer(uint32_t addr) {
	if (buffer && reinterpret_cast<uintptr_t>(buffer) != -1) {
		std::lock_guard lock(bufferMutex);
		return buffer[addr];
	}
	return read(mcu, addr);
}

void HexGrid::WriteBuffer(uint32_t addr, uint8_t val) {
	if (buffer && reinterpret_cast<uintptr_t>(buffer) != -1) {
		std::lock_guard lock(bufferMutex);
		buffer[addr] = val;
	} else write(mcu, addr, val);
}

HexView::HexView(wxWindow* parent, MCU *mcu)
	: wxPanel(parent)
{
	header = new HexHeader(this);
	grid   = new HexGrid(this, mcu);

	auto* sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(header, 0, wxEXPAND);
	sizer->Add(grid, 1, wxEXPAND);
	SetSizer(sizer);
}

void HexView::SetBuffer(uint8_t* buf, size_t size, uint32_t baseAddr) {
	grid->SetBuffer(buf, size, baseAddr);
}

void HexView::SetBuffer(readtype _read, writetype _write, size_t size, uint32_t baseAddr) {
	grid->SetBuffer(_read, _write, size, baseAddr);
}
