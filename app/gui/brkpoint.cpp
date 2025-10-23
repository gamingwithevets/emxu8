#include "brkpoint.h"
#include "base/regcheckbox.h"
#include "base/regtextctrl.h"

Brkpoint::Brkpoint() {
	Create(nullptr);
}

void Brkpoint::Open() {
	Show();
	Raise();
}

void Brkpoint::Sync() {
	for (auto &panel : brkpoints) panel->Sync();
}


void Brkpoint::OnBtnAdd(wxCommandEvent &) {
	auto *panel = new BrkpointPanel(this);
	panel->m_btn_delete->Bind(wxEVT_BUTTON, &Brkpoint::OnBtnDelete, this);
	brkpoints.push_back(panel);
	m_sizer->Add(panel, wxSizerFlags());
	m_sizer->Layout();
}

void Brkpoint::OnBtnClear(wxCommandEvent &) {
	for (auto &panel : brkpoints) {
		m_sizer->Detach(panel);
		panel->Destroy();
	}
	m_sizer->Layout();
	brkpoints.clear();
}

void Brkpoint::OnBtnDelete(wxCommandEvent &event) {
	auto btn = dynamic_cast<wxButton *>(event.GetEventObject());
	if (btn) {
		auto panel = btn->GetParent();
		m_sizer->Detach(panel);
		panel->Destroy();
		m_sizer->Layout();
		brkpoints.erase(std::ranges::remove(brkpoints, panel).begin(), brkpoints.end());
	}
}

BrkpointPanel::BrkpointPanel(wxWindow* parent) : BrkpointPanelBase(parent) {
	m_enabled->BindToFlag(&enabled);
	m_csr->BindToRegister(&seg);
	m_pc->BindToRegister(&addr);
}

void BrkpointPanel::Sync() {
	m_enabled->SyncFromCore();
	m_csr->SyncFromCore();
	m_pc->SyncFromCore();
}

void BrkpointPanel::ChangeType(Type _type) {
	type = _type;
	if (type != EXECUTE) {
		m_csr->SetConfig({false, 2});
		m_pc->SetConfig({false, 4});
	} else {
		m_csr->SetConfig({false, 1});
		m_pc->SetConfig({true, 4});
	}
}
