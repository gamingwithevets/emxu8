// Created with ChatGPT

#pragma once
#include <wx/wx.h>

class RegCheckBox : public wxCheckBox {
public:
	std::function<bool()> getter;
	std::function<void(bool)> setter;

	RegCheckBox(wxWindow* parent, wxWindowID id, const wxString& label)
		: wxCheckBox(parent, id, label)
	{
		Bind(wxEVT_CHECKBOX, &RegCheckBox::OnToggle, this);
	}

	void BindToFlag(std::function<bool()> g,
					std::function<void(bool)> s) {
		getter = g;
		setter = s;
	}

	// 🆕 convenience overload for plain bool flags
	void BindToFlag(bool* flag) {
		getter = [=] { return *flag; };
		setter = [=](bool v) { *flag = v; };
	}

	void SyncFromCore() {
		if (getter) SetValue(getter());
	}

private:
	void OnToggle(wxCommandEvent& evt) {
		if (setter) setter(GetValue());
		evt.Skip();
	}
};
