#pragma once
#include <vector>
#include <wx/wx.h>
#include "../mcu/mcu.h"
#include "base/hexview.h"

struct Buffer {
	const char* name;
	uint8_t* data;
	size_t size;
	uint32_t baseAddress;
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
