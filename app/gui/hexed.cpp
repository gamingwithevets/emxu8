#include "hexed.h"
#include <wx/splitter.h>
#include <cstring>

HexEditorDialog::HexEditorDialog(MCU *_mcu)
	: mcu(_mcu), wxDialog(nullptr, wxID_ANY,
			   "Hex Editor",
			   wxDefaultPosition, wxSize(815, 600),
			   wxDEFAULT_DIALOG_STYLE)
{

	buffers.push_back({"RAM space", mcu->ram, mcu->ramsize, mcu->ramstart});
	buffers.push_back({"SFR region", mcu->core.sfrs, 0x1000, 0xf000});

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

	hex = new HexView(this);

	vbox->Add(combo, 0, wxEXPAND | wxALL, 4);
	vbox->Add(hex, 1, wxEXPAND | wxALL, 4);

	SetSizer(vbox);

	for (auto& b : buffers)
		combo->Append(b.name);

	combo->Bind(wxEVT_COMBOBOX, [&](wxCommandEvent& e) {
		auto& b = buffers[e.GetSelection()];
		hex->SetBuffer(b.data, b.size, b.baseAddress);
		hex->SetFocus();
	});
	combo->Bind(wxEVT_COMBOBOX_CLOSEUP, [&](wxCommandEvent& e) {
		hex->SetFocus();
		e.Skip();
	});

	combo->SetSelection(0);
	hex->SetBuffer(buffers[0].data, buffers[0].size, buffers[0].baseAddress);
	hex->SetFocus();
}

void HexEditorDialog::Open() {
	Show();
	Raise();
}

