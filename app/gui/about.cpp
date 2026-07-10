#include "about.h"
#include "..\ver.h"
#include <emxu8/emxu8.h>
#include <cstdlib>

AboutDialog::AboutDialog(wxWindow *parent) {
	Create(parent);
	const char date[] = __DATE__;
	uint16_t year = strtoul(&date[7], nullptr, 10);
	char year_str[11] = "2025";
	if (year > 2025) sprintf(&year_str[4], "-%u", year);
	m_version->SetLabel(
		wxString("Version ") + VERSION +
		"\nCore version " + EMXU8_CORE_VERSION +
		wxString::FromUTF8("\n\n© ") + year_str + " GamingWithEvets Inc.\n"
	);
}

void AboutDialog::Open() {
	ShowModal();
}
