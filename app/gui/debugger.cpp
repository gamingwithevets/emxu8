#include <string_view>
#include "debugger.h"
#include "base/regtextctrl.h"
#include "base/regcheckbox.h"

template<typename T>
constexpr static T from_buf(const uint8_t* data) {
	T value = 0;
	for (size_t i = 0; i < sizeof(T); i++) value |= static_cast<T>(data[i]) << (8 * i);
	return value;
}

Debugger::Debugger(emxu8::U8Core *_core, bool single_step) : core(_core) {
	Create(nullptr);
	SetStepButtons(single_step);

	wxAcceleratorEntry entries[3];
	entries[0].Set(wxACCEL_NORMAL, WXK_F9, m_btn_run->GetId());
	entries[1].Set(wxACCEL_NORMAL, WXK_F7, m_btn_stepinto->GetId());
	entries[2].Set(wxACCEL_NORMAL, WXK_F8, m_btn_stepover->GetId());
	wxAcceleratorTable accel(sizeof(entries) / sizeof(wxAcceleratorEntry), entries);
	SetAcceleratorTable(accel);

	m_r_sizer->AddStretchSpacer();
	for (uint8_t i = 0; i < 16; i++) {
		auto *r_sizer = new wxBoxSizer(wxVERTICAL);
		auto *r = new RegTextCtrl(this);
		r->Enable(false);
		r->SetMinSize(ConvertDialogToPixels(wxSize(15, -1)));
		{
			wxFontInfo font_info(10);
			font_info.Family(wxFONTFAMILY_TELETYPE);
			r->SetFont(wxFont(font_info));
		}
		r->BindToRegister(&core->r[i]);
		m_r[i] = r;
		r_sizer->Add(r, wxSizerFlags().Center());
		auto *r_text = new wxStaticText(this, wxID_ANY, wxString::Format("R%u", i));
		r_sizer->Add(r_text, wxSizerFlags().Center());
		m_r_sizer->Add(r_sizer, wxSizerFlags().Border(wxALL));
	}
	m_r_sizer->AddStretchSpacer();

	m_next_dsr->BindToFlag([&]{ return core->executor->next_dsr > 0; }, [&](bool){ core->executor->next_dsr = 1; });

	m_csr->BindToRegister([&] { return core->csr; }, [&](const uint8_t val) { core->csr = val; });
	m_pc->BindToRegister(&core->pc);
	m_sp->BindToRegister(&core->sp);
	m_dsr->BindToRegister(&core->dsr);
	m_ea->BindToRegister(&core->ea);
	m_psw->BindToRegister(&core->psw.raw);
	m_psw_c->BindToFlag([&]{ return core->psw.c; }, [&](const bool val) { core->psw.c = val; });
	m_psw_z->BindToFlag([&]{ return core->psw.z; }, [&](const bool val) { core->psw.z = val; });
	m_psw_s->BindToFlag([&]{ return core->psw.s; }, [&](const bool val) { core->psw.s = val; });
	m_psw_ov->BindToFlag([&]{ return core->psw.ov; }, [&](const bool val) { core->psw.ov = val; });
	m_psw_mie->BindToFlag([&]{ return core->psw.mie; }, [&](const bool val) { core->psw.mie = val; });
	m_psw_hc->BindToFlag([&]{ return core->psw.hc; }, [&](const bool val) { core->psw.hc = val; });
	m_psw_elevel->BindToRegister([&]{ return core->psw.elevel; }, [&](const uint8_t val) { core->psw.elevel = val; });

	m_lr_sizer->AddStretchSpacer();
	for (uint8_t i = 0; i < 4; i++) {
		auto *lr_text = new wxStaticText(this, wxID_ANY, i == 0 ? "LCSR:LR  " : wxString::Format("  ECSR%u:ELR%u  ", i, i));
		m_lr_sizer->Add(lr_text, wxSizerFlags().Center());
		auto *lcsr = new RegTextCtrl({false, 1}, this);
		lcsr->Enable(false);
		lcsr->SetMinSize(ConvertDialogToPixels(wxSize(10, -1)));
		{
			wxFontInfo font_info(10);
			font_info.Family(wxFONTFAMILY_TELETYPE);
			lcsr->SetFont(wxFont(font_info));
		}
		lcsr->BindToRegister(&core->ecsr[i]);
		m_lcsr[i] = lcsr;
		m_lr_sizer->Add(lcsr, wxSizerFlags().Center());

		auto* lr_sep = new wxStaticText(this, wxID_ANY, _(" : "));
		m_lr_sizer->Add(lr_sep, wxSizerFlags().Center());

		auto *lr = new RegTextCtrl({true, 4}, this);
		lr->Enable(false);
		lr->SetMinSize(ConvertDialogToPixels(wxSize(25, -1)));
		{
			wxFontInfo font_info(10);
			font_info.Family(wxFONTFAMILY_TELETYPE);
			lr->SetFont(wxFont(font_info));
		}
		lr->BindToRegister(&core->elr[i]);
		m_lr[i] = lr;
		m_lr_sizer->Add(lr, wxSizerFlags().Center());
	}
	m_lr_sizer->AddStretchSpacer();

	m_epsw_sizer->AddStretchSpacer();
	for (uint8_t i = 1; i < 4; i++) {
		auto *epsw_text = new wxStaticText(this, wxID_ANY, wxString::Format("  EPSW%u  ", i));
		m_epsw_sizer->Add(epsw_text, wxSizerFlags().Center());
		auto *epsw = new RegTextCtrl(this);
		epsw->Enable(false);
		epsw->SetMinSize(ConvertDialogToPixels(wxSize(15, -1)));
		{
			wxFontInfo font_info(10);
			font_info.Family(wxFONTFAMILY_TELETYPE);
			epsw->SetFont(wxFont(font_info));
		}
		epsw->BindToRegister(&core->epsw[i]);
		m_epsw[i-1] = epsw;
		m_epsw_sizer->Add(epsw, wxSizerFlags().Center());
	}
	m_epsw_sizer->AddStretchSpacer();

	disassembler = new emxu8::U8Disassembler(core);
}

void Debugger::Open() {
	Sync();
	Show();
	Raise();
}

void Debugger::Sync() {
	for (uint8_t i = 0; i < 16; i++) m_r[i]->SyncFromCore();
	m_csr->SyncFromCore();
	m_pc->SyncFromCore();
	m_sp->SyncFromCore();
	m_dsr->SyncFromCore();
	m_ea->SyncFromCore();
	m_next_dsr->SyncFromCore();
	m_psw->SyncFromCore();
	m_psw_c->SyncFromCore();
	m_psw_z->SyncFromCore();
	m_psw_s->SyncFromCore();
	m_psw_ov->SyncFromCore();
	m_psw_mie->SyncFromCore();
	m_psw_hc->SyncFromCore();
	m_psw_elevel->SyncFromCore();
	for (uint8_t i = 0; i < 4; i++) {
		m_lcsr[i]->SyncFromCore();
		m_lr[i]->SyncFromCore();
		if (i > 0) m_epsw[i-1]->SyncFromCore();
	}

	std::string disas;
	uint32_t pc;
	if (core->executor->cur_pc <= 0xfffff) pc = core->executor->cur_pc;
	else if (core->decoder->cur_pc <= 0xfffff) pc = core->decoder->cur_pc;
	else pc = core->csr << 16 | core->pc;
	for (uint16_t i = 0; i < 17; i++) {
		uint8_t ins_len;
		std::string disas_str = disassembler->Disassemble(pc & 0xfffe, pc >> 16, ins_len);
		if (single_step) {
			if (pc == core->executor->cur_pc) disas += "EX> ";
			else if (pc == core->decoder->cur_pc) disas += "ID> ";
			else if (pc == (core->csr << 16 | core->pc)) disas += "IF> ";
			else disas += "    ";
		} else disas += "    ";
		disas += std::format("{:05X}    ", pc);
		for (uint16_t j = 0; j < 6; j += 2) disas += j >= ins_len ? "    " : std::format("{:04X}", static_cast<unsigned int>(core->ReadCodeMemory(pc + j & 0xfffe, pc >> 16)));
		disas += "    " + disas_str + "\n";
		pc += ins_len;
	}
	m_disas->Clear();
	m_disas->SetValue(disas);

	std::vector<emxu8::U8Interrupter::call> callstackv;
	{
		std::lock_guard lock(core->mutex);
		callstackv.reserve(core->interrupter->callstack.size());
		callstackv.insert(callstackv.end(), core->interrupter->callstack.begin(), core->interrupter->callstack.end());
	}
	std::string callstack;
	size_t cssize = callstackv.size();
	callstack += std::format("{} calls\n\n", cssize);
	for (size_t i = 0; i < cssize; i++) {
		auto &callitem = callstackv[cssize - 1 - i];
		callstack += std::format("#{}\n-> {:05X}\n<- ", i, callitem.func_addr);
		if (callitem.func_addr >= 0x100000) continue;
		if (callitem.intr.vector_addr >= 4) callstack += std::string("[") + std::string(callitem.intr.vector_addr == 6 ? "NMICE" : callitem.intr.vector_addr <= 8 ? "NMI" : "MI") + ": " + callitem.intr.name + "]";
		else if (callitem.lr_loc) {
			uint8_t buf[3]{};
			core->ReadDataMemory(callitem.lr_loc, 0, core->GetMemoryModel() == emxu8::MM_LARGE ? 3 : 2, buf);
			auto ret_addr = from_buf<uint32_t>(buf);
			ret_addr &= 0xffffe;
			callstack += std::format("{:05X} (LR pushed @ {:04X})", ret_addr, static_cast<unsigned int>(callitem.lr_loc));
		} else callstack += std::format("{:05X}", callitem.ret_addr);
		callstack += "\n\n";
	}
	m_call_stack->SetValue(callstack);

	m_stack_raw->SetValue("The raw stack display isn't available\nin this version. Sorry!");
}

void Debugger::SetStepButtons(bool enable) {
	m_btn_run->Enable(enable);
	m_btn_stepinto->Enable(enable);
	m_btn_stepover->Enable(enable);
	single_step = enable;
}
