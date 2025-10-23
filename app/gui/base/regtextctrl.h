// (Mostly) Created with ChatGPT

#pragma once
#include <wx/wx.h>
#include <functional>

struct RegInputConfig {
    bool wordAligned;     // enforce even addresses (16-bit only)
    int  maxDigits;       // 2 = 8-bit, 4 = 16-bit, 1 = mode field
    bool modeDigitsOnly;  // true = only allow 0–3
};

class RegTextCtrl : public wxTextCtrl {
    RegInputConfig cfg{false, 2, false};

public:
    // 🆕 getter/setter for binding
    std::function<unsigned long()> getter;
    std::function<void(unsigned long)> setter;

    // wxWidgets-normal constructor
    RegTextCtrl(wxWindow* parent,
                wxWindowID id = wxID_ANY,
                const wxString& value = "",
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxSize(80, -1),
                long style = wxTE_RIGHT,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxTextCtrlNameStr,
                const RegInputConfig& c = {false, 2, false})
        : wxTextCtrl(parent, id, value, pos, size, style, validator, name),
          cfg(c)
    {
        Init();
    }

    // wxUiEditor-friendly constructor (cfg first)
    RegTextCtrl(const RegInputConfig& c,
                wxWindow* parent,
                wxWindowID id = wxID_ANY,
                const wxString& value = "",
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxSize(80, -1),
                long style = wxTE_RIGHT,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxTextCtrlNameStr)
        : wxTextCtrl(parent, id, value, pos, size, style, validator, name),
          cfg(c)
    {
        Init();
    }

    // 🆕 bind with getter/setter lambdas
    void BindToRegister(std::function<unsigned long()> g,
                        std::function<void(unsigned long)> s)
    {
        getter = g;
        setter = s;
    }

    // 🆕 convenience overload for plain variables (8- or 16-bit)
    template<typename T>
    void BindToRegister(T* reg) {
        static_assert(sizeof(T) == 1 || sizeof(T) == 2,
                      "Only 8-bit or 16-bit registers supported!");
        getter = [=] { return (unsigned long)*reg; };
        setter = [=](unsigned long v) { *reg = (T)v; };
    }

    // Pulls value from core into text field
    void SyncFromCore() {
        if (!getter) return;

        unsigned long number = getter();

        if (cfg.maxDigits == 2)
            number &= 0xFF;
        else
            number &= 0xFFFF;

        if (cfg.wordAligned)
            number &= ~1UL;

        ChangeValue(wxString::Format("%0*lX", cfg.maxDigits, number));
    }

public:
	// 🆕 update configuration at runtime
	void SetConfig(const RegInputConfig& newCfg) {
		cfg = newCfg;
		ReapplyConfig();
	}

	const RegInputConfig& GetConfig() const {
		return cfg;
	}

private:
	void Init() {
		ReapplyConfig();

		Bind(wxEVT_KILL_FOCUS, &RegTextCtrl::OnCommit, this);
		Bind(wxEVT_TEXT_ENTER, &RegTextCtrl::OnCommit, this);
	}

	void ReapplyConfig() {
		// reset length
		SetMaxLength(cfg.maxDigits);

		// rebuild validator
		if (cfg.modeDigitsOnly) {
			wxTextValidator validator(wxFILTER_INCLUDE_CHAR_LIST);
			validator.SetIncludes({"0","1","2","3"});
			SetValidator(validator);
		} else {
			wxString hexChars = "0123456789ABCDEFabcdef";
			wxArrayString includes;
			for (wxUniChar ch : hexChars)
				includes.Add(wxString(ch));

			wxTextValidator validator(wxFILTER_INCLUDE_CHAR_LIST);
			validator.SetIncludes(includes);
			SetValidator(validator);
		}

		// force sync if bound
		SyncFromCore();
	}

	void OnCommit(wxEvent& evt) {
		if (!setter) { evt.Skip(); return; }

		unsigned long number = 0;

		if (cfg.modeDigitsOnly) {
			if (!GetValue().IsEmpty())
				number = GetValue()[0] - '0';
		} else {
			if (!GetValue().ToULong(&number, 16)) {
				evt.Skip();
				return;
			}
		}

		// mask by number of hex digits
		unsigned long mask = (cfg.maxDigits >= 4) ? 0xFFFFUL
							: (1UL << (cfg.maxDigits * 4)) - 1;
		number &= mask;

		if (cfg.wordAligned)
			number &= ~1UL;

		ChangeValue(wxString::Format("%0*lX", cfg.maxDigits, number));
		setter(number);

		evt.Skip();
	}
};
