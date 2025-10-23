#pragma once

#include "base/debugger.h"
#include <emxu8/emxu8.h>
#include <emxu8/disassembler.h>

class Debugger : public DebuggerBase {
	emxu8::U8Core *core;
	emxu8::U8Disassembler *disassembler;
	RegTextCtrl *m_r[16]{};
	RegTextCtrl *m_lcsr[4]{};
	RegTextCtrl *m_lr[4]{};
	RegTextCtrl *m_epsw[3]{};
public:
	bool single_step = false;
	Debugger(emxu8::U8Core *_core, bool single_step);
	void Open();
	void Sync();
	void SetStepButtons(bool enable);
};
