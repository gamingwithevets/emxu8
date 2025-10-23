#pragma once

#include "base/about.h"

class AboutDialog : AboutDialogBase {
public:
	AboutDialog(wxWindow* parent);
	void Open();
};
