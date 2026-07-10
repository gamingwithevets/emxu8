#include "hexed.h"
#include <wx/splitter.h>
#include <mutex>

uint8_t read_sfr(MCU *mcu, uint16_t addr) {
	std::lock_guard lock(mcu->core.mutex);
	uint8_t out;
	mcu->core.ReadDataMemory(addr + 0xf000, 0, 1, &out);
	return out;
}

void write_sfr(MCU *mcu, uint16_t addr, uint8_t val) {
	std::lock_guard lock(mcu->core.mutex);
	mcu->core.WriteDataMemory(addr + 0xf000, 0, 1, &val);
}

HexEditorDialog::HexEditorDialog(MCU *_mcu)
	: wxDialog(nullptr, wxID_ANY,
			   "Hex Editor",
			   wxDefaultPosition, wxSize(815, 610),
			   wxDEFAULT_DIALOG_STYLE),
	  mcu(_mcu)
{

	buffers.push_back({"RAM space", mcu->ramsize, mcu->ramstart, mcu->ram});
	buffers.push_back({"SFR region", 0x1000, 0xf000, nullptr, &read_sfr, &write_sfr});

	auto* vbox = new wxBoxSizer(wxVERTICAL);

	combo = new wxComboBox(
		this,
		wxID_ANY,
		"",
		wxDefaultPosition,
		wxDefaultSize,
		0,
		nullptr,
		wxCB_READONLY
	);

	hex = new HexView(this, mcu);

	vbox->Add(combo, 0, wxEXPAND | wxALL, 4);
	vbox->Add(hex, 1, wxEXPAND | wxALL, 4);

	SetSizer(vbox);

	for (auto& b : buffers)
		combo->Append(b.name);

	combo->Bind(wxEVT_COMBOBOX, [&](wxCommandEvent& e) {
		auto& b = buffers[e.GetSelection()];
		if (b.data) hex->SetBuffer(b.data, b.size, b.baseAddress);
		else hex->SetBuffer(b.read, b.write, b.size, b.baseAddress);
		hex->SetFocus();
	});
	combo->Bind(wxEVT_COMBOBOX_CLOSEUP, [&](wxCommandEvent& e) {
		hex->SetFocus();
		e.Skip();
	});

	combo->SetSelection(0);
	if (buffers[0].data) hex->SetBuffer(buffers[0].data, buffers[0].size, buffers[0].baseAddress);
	else hex->SetBuffer(buffers[0].read, buffers[0].write, buffers[0].size, buffers[0].baseAddress);
	hex->SetFocus();
}

void HexEditorDialog::Open() {
	Show();
	Raise();
}
