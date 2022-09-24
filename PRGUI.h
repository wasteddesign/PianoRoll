#pragma once

#include "MachinePattern.h"

using namespace System;
using namespace System::Windows::Controls;
using namespace System::Windows::Threading;
using namespace System::Windows::Interop;
using namespace System::Runtime::InteropServices; 
using namespace Pianoroll::GUI;


ref class ManagedGUI : public IGUICallbacks
{
public:
	ManagedGUI(NativeData *pnd)
	{
		pnd->TargetMac = NULL;
		this->pnd = pnd;
		Control = gcnew Pianoroll::GUI::Editor(this);
	}

    virtual void WriteDC(String ^text)
	{
#ifdef _DEBUG
		const char* str2 = (char*)(void*)Marshal::StringToHGlobalAnsi(text);
		pnd->pCB->WriteLine(str2);
		Marshal::FreeHGlobal((System::IntPtr)(void*)str2);
#endif
	}

	virtual int GetPlayPosition()
	{
		return pnd->PlayPos;
	}

    virtual bool GetPlayNotesState()
	{
		return pnd->pCB->GetPlayNotesState();
	}

  
    virtual void MidiNote(int note, int velocity)
	{
		if (pnd->TargetMac == NULL)
			return;

		pnd->pCB->SendMidiNote(pnd->TargetMac, 0, note, velocity);
		(pnd->pMI->*pnd->recordCallback)(note, velocity);
	}

    virtual int GetBaseOctave()
	{
		return pnd->pCB->GetBaseOctave();
	}

    virtual bool IsMidiNoteImplemented()
	{
		return pnd->pCB->MachineImplementsFunction(pnd->TargetMac, 12, false);
	}

	virtual void GetGUINotes()
	{
		vector<NoteToGUI> e;

		::EnterCriticalSection(&pnd->PatternCS);
		e = pnd->notesToGUI;
		pnd->notesToGUI.clear();
		::LeaveCriticalSection(&pnd->PatternCS);
		
		for (int i = 0; i < (int)e.size(); i++)
			Control->MidiNote(e[i].e.D1, e[i].e.D2, e[i].FromUser);	

	}

	virtual void GetRecordedNotes()
	{
		vector<pair<int, MTEvent>> e;

		::EnterCriticalSection(&pnd->PatternCS);
		e = pnd->recNotesToGUI;
		pnd->recNotesToGUI.clear();
		::LeaveCriticalSection(&pnd->PatternCS);
		
		for (int i = 0; i < (int)e.size(); i++)
			Control->AddRecordedNote(NoteEvent(e[i].first, e[i].second.Length, e[i].second.D1, e[i].second.D2));

	}

    virtual int GetStateFlags()
	{
		return pnd->pCB->GetStateFlags();
	}

	virtual bool TargetSet() { return pnd->TargetMac != NULL; }

	virtual void SetStatusBarText(int pane, String ^text)
	{
		const char *s = (char*)(void*)Marshal::StringToHGlobalAnsi(text);
		pnd->pCB->SetPatternEditorStatusText(pane, s);
		Marshal::FreeHGlobal((System::IntPtr)(void*)s);
	}

	virtual void PlayNoteEvents(IEnumerable<NoteEvent> ^notes)
	{
		if (!pnd->pCB->GetPlayNotesState())
			return;

		MapIntToMTEvent n;

		for each (NoteEvent e in notes)
			n[pair<int, int>(e.Time, e.Note)] = MTEvent(e.Note, e.Velocity, e.Length);

		pnd->playingNoteSet->Play(n);
	}

	virtual bool IsEditorWindowVisible()
	{
		HWND hwnd = (HWND)HS->Handle.ToPointer();
		HDC hdc = ::GetDC(hwnd);
		RECT r;
		bool visible = ::GetClipBox(hdc, &r) != NULLREGION;
		::ReleaseDC(hwnd, hdc);
		return visible;
	}

	virtual String ^GetThemePath()
	{
		return gcnew String(pnd->pCB->GetThemePath());
	}

public:
	void SetEditorPattern(CMachinePattern *p)
	{
		pEditorPattern = p;
		Control->Pattern = p != NULL ? p->MPattern : nullptr;
	}

	void SetTargetMachine(CMachine *pmac)
	{
		pnd->TargetMac = pmac;
		Control->TargetMachineChanged();
	}



public:
	HwndSource ^HS;
	Editor ^Control;
	CMachinePattern *pEditorPattern;
	NativeData *pnd;


};


class GUI
{
public:
	gcroot<ManagedGUI ^> MGUI;
	HWND Window;

	// these wrappers are here to keep Pianoroll.cpp completely unmanaged

	void MachineDestructor()
	{
		MGUI->Control->MachineDestructor();
	}

	void SetEditorPattern(CMachinePattern *pmp)
	{
		MGUI->SetEditorPattern(pmp);
	}

	void SetTargetMachine(CMachine *pmac)
	{
		MGUI->SetTargetMachine(pmac);
	}

	void ShowHelp()
	{
		MGUI->Control->ShowHelp();
	}

	void SetBaseOctave(int bo)
	{
		MGUI->Control->SetBaseOctave(bo);
	}

	int GetEditorPatternPosition()
	{
		return MGUI->Control->GetEditorPatternPosition();
	}

	void Update()
	{
		MGUI->Control->Update();
	}

	void ThemeChanged()
	{
		MGUI->Control->ThemeChanged();
	}
};

extern void CreateGUI(GUI &gui, HWND parent, NativeData *pnd);
