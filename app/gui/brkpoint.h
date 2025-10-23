#pragma once

#include "base/brkpoint.h"
#include "base/brkpoint_panel.h"

class BrkpointPanel : public BrkpointPanelBase
{
public:
	enum Type {
		EXECUTE,
		READ,
		WRITE
	};
	BrkpointPanel(wxWindow* parent);
	bool enabled = true;
	uint8_t seg = 0;
	uint16_t addr = 0;
	Type type = EXECUTE;
	void Sync();
	void ChangeType(Type _type);
	void OnRadioType0(wxCommandEvent &) override { ChangeType(EXECUTE); }
	void OnRadioType1(wxCommandEvent &) override { ChangeType(READ); }
	void OnRadioType2(wxCommandEvent &) override { ChangeType(WRITE); }
};

class Brkpoint : public BrkpointBase
{
public:
	std::vector<BrkpointPanel *> brkpoints;

    Brkpoint();
	void Open();
private:
	void Sync();
	void OnBtnAdd(wxCommandEvent &) override;
	void OnBtnClear(wxCommandEvent &) override;
	void OnBtnDelete(wxCommandEvent &event);
};
