#pragma once
#include <vector>
#include <wx/wx.h>
#include "../mcu/mcu.h"
#include "base/hexview.h"

struct Buffer {
	const char* name;
	size_t size;
	uint32_t baseAddress;
	uint8_t* data;
	readtype read;
	writetype write;
};

class HexEditorDialog : public wxDialog {
	MCU *mcu;
	wxComboBox *combo;
	HexView* hex;
	std::vector<Buffer> buffers;
public:
	HexEditorDialog(MCU *);
	void Open();
};
