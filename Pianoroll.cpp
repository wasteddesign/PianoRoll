#include "stdafx.h"
#include "PRGUI.h"
#include "Compress.h"

#pragma unmanaged

#define CHECK_BUILD_NUMBER

#ifdef CHECK_BUILD_NUMBER
static char const *BuildNumber =
#include "../../buildcount"
;
#endif

#define PIANOROLL_TPB		960

CMachineParameter const paraDummy = 
{ 
	pt_byte,										// type
	"Dummy",
	"Dummy",							// description
	0,												// MinValue	
	127,											// MaxValue
	255,												// NoValue
	MPF_STATE,										// Flags
	0
};


static CMachineParameter const *pParameters[] = { 
	// track
	&paraDummy,
};

#pragma pack(1)

class gvals
{
public:
	byte dummy;

};

#pragma pack()

CMachineInfo const MacInfo = 
{
	MT_GENERATOR,							// type
	MI_VERSION,
	MIF_PATTERN_EDITOR | MIF_NO_OUTPUT | MIF_CONTROL_MACHINE | MIF_PE_NO_CLIENT_EDGE,						// flags	
	0,											// min tracks
	0,								// max tracks
	1,										// numGlobalParameters
	0,										// numTrackParameters
	pParameters,
	0, 
	NULL,
	"Jeskola Pianoroll",
	"Pianoroll",								// short name
	"Oskari Tammelin", 						// author
	NULL
};

class mi;

class miex : public CMachineInterfaceEx
{
public:
	virtual void *CreatePatternEditor(void *parenthwnd);
	virtual void CreatePattern(CPattern *p, int numrows);
	virtual void CreatePatternCopy(CPattern *pnew, CPattern const *pold);
	virtual void DeletePattern(CPattern *p);
	virtual void RenamePattern(CPattern *p, char const *name);
	virtual void SetPatternLength(CPattern *p, int length);
	virtual void PlayPattern(CPattern *p, CSequence *s, int offset);
	virtual void SetEditorPattern(CPattern *p);
	virtual void SetPatternTargetMachine(CPattern *p, CMachine *pmac);
	virtual bool ShowPatternEditorHelp();
	virtual void SetBaseOctave(int bo);
	virtual bool EnableCommandUI(int id);
	virtual int GetEditorPatternPosition();
	virtual void GotMidiFocus();
	virtual void LostMidiFocus();
	virtual void MidiControlChange(int const ctrl, int const channel, int const value);
	virtual void ThemeChanged();

	bool ExportMidiEvents(CPattern *p, CMachineDataOutput *pout);
	bool ImportMidiEvents(CPattern *p, CMachineDataInput *pin);

public:
	mi *pmi;

};
class mi : public CMachineInterface
{
public:
	mi();
	virtual ~mi();

	virtual void Init(CMachineDataInput * const pi);
	virtual void Tick();
	virtual bool Work(float *pout, int numsamples, int const mode);
	virtual void Save(CMachineDataOutput * const po);
	virtual void MidiNote(int const channel, int const value, int const velocity);

	bool DClickMachine(void *)
	{
		pCB->SetPatternEditorMachine(ThisMac, true);
		return true;
	}

	virtual void Stop();

	void StopPlayingNotes();

	void AddToRecBuffer(int key, int vel);

	void GotMidiFocus();
	void LostMidiFocus();

private:


public:
	miex ex;
	CMachine *ThisMac;
	CMachine *TargetMac;
	MapPatternToMachinePattern patterns;
	MapStringToMachinePattern loadedPatterns;
	CMachinePattern *pPlayingPattern;
	CMachinePattern *pEditorPattern;
	int patternPos;
	int posInTick;
	NativeData nd;
	MapIntToMTEvent playingNotes;
	map<int, MTEvent> recordingNotes;
	vector<MTEvent> noteOnRecBuffer;
	vector<MTEvent> noteOffRecBuffer;
	set<int> activeMidiNotes;
	CRITICAL_SECTION amnCS;
	bool recording;

	GUI gui;
	gvals gval;

};

void miex::GotMidiFocus() { pmi->GotMidiFocus(); }
void miex::LostMidiFocus() { pmi->LostMidiFocus(); }

bool miex::ExportMidiEvents(CPattern *p, CMachineDataOutput *pout) { return pmi->patterns[p]->ExportMidiEvents(pout); }
bool miex::ImportMidiEvents(CPattern *p, CMachineDataInput *pin) { return pmi->patterns[p]->ImportMidiEvents(pin); }

void miex::ThemeChanged() { pmi->gui.ThemeChanged(); }

DLL_EXPORTS

extern void SetResolveEventHandler();

mi::mi()
{
	ex.pmi = this;
	GlobalVals = &gval;
	TrackVals = NULL;
	AttrVals = NULL;
	pPlayingPattern = NULL;
	TargetMac = NULL;
	recording = false;
	::InitializeCriticalSection(&nd.PatternCS);
	::InitializeCriticalSection(&amnCS);

	SetResolveEventHandler();
}

mi::~mi()
{
	if (gui.Window != NULL)
	{
		gui.MachineDestructor();
		::DestroyWindow(gui.Window);
	}

	::DeleteCriticalSection(&nd.PatternCS);
	::DeleteCriticalSection(&amnCS);
}

void mi::Init(CMachineDataInput * const pi)
{
#ifdef CHECK_BUILD_NUMBER
	int hostbn = pCB->GetBuildNumber();
	int pxpbn = atoi(BuildNumber);
	if (hostbn != pxpbn)
	{
		char s[256];
		sprintf_s(s, 256, "Incompatible build (Host %d, Pianoroll %d)", hostbn, pxpbn);
		::MessageBox(NULL, s, "Pianoroll", MB_OK | MB_ICONWARNING);
	}

#endif

	nd.pMI = this;
	nd.pCB = pCB;
	nd.PlayPos = -1;
	nd.recordCallback = static_cast<RecordCallback>(&mi::AddToRecBuffer);
	nd.playingNoteSet = shared_ptr<CPlayingNoteSet>(new CPlayingNoteSet(&nd));
	
	pCB->SetMachineInterfaceEx(&ex);
	ThisMac = pCB->GetThisMachine();
	pCB->SetEventHandler(ThisMac, DoubleClickMachine, (EVENT_HANDLER_PTR)&mi::DClickMachine, NULL);

	if (pi != NULL)
	{
		byte version;
		pi->Read(version);
		if (version != PIANOROLL_DATA_VERSION)
		{
//			AfxMessageBox("invalid data");
			return;
		}

		int numpat;
		pi->Read(numpat);

		for (int i = 0; i < numpat; i++)
		{
			string name;

			while(true)
			{
				char ch;
				pi->Read(ch);
				if (ch == 0)
					break;
				name += ch;
			}

			shared_ptr<CMachinePattern> p(new CMachinePattern(&nd));
			p->Read(pi);
			loadedPatterns[name] = p;
			
		}
	}
	

}

void mi::Save(CMachineDataOutput * const po)
{
	byte version = PIANOROLL_DATA_VERSION;
	po->Write(version);
	po->Write((int)patterns.size());

	for (MapPatternToMachinePattern::iterator i = patterns.begin(); i != patterns.end(); i++)
	{
		char const *name = pCB->GetPatternName((*i).first);
		po->Write(name);
		(*i).second->Write(po);
	}
}


void *miex::CreatePatternEditor(void *parenthwnd)
{
	pmi->gui.Window = 0;
	CreateGUI(pmi->gui, (HWND)parenthwnd, &pmi->nd);

	return pmi->gui.Window;
}

void miex::CreatePattern(CPattern *p, int numrows)
{
	MapStringToMachinePattern::iterator i = pmi->loadedPatterns.find(pmi->pCB->GetPatternName(p));

	if (i != pmi->loadedPatterns.end())
	{
//		(*i).second->pPattern = p;
		pmi->patterns[p] = (*i).second;
		pmi->patterns[p]->Init(p, numrows, &pmi->nd);
		pmi->loadedPatterns.erase(i);
	}
	else
	{
		pmi->patterns[p] = shared_ptr<CMachinePattern>(new CMachinePattern(p, numrows, &pmi->nd));
	}
	
}

void miex::CreatePatternCopy(CPattern *pnew, CPattern const *pold)
{
	pmi->patterns[pnew] = shared_ptr<CMachinePattern>(new CMachinePattern(pnew, pmi->patterns[(CPattern *)pold].get(), &pmi->nd));
}

void miex::DeletePattern(CPattern *p)
{
	if (pmi->pPlayingPattern == pmi->patterns[p].get())
		pmi->pPlayingPattern = NULL;

	pmi->patterns.erase(pmi->patterns.find(p));
}

void miex::RenamePattern(CPattern *p, char const *name)
{
	// this is only needed if you want to display the name
}

void miex::SetPatternLength(CPattern *p, int length)
{
	pmi->patterns[p]->SetLength(length);
}

void miex::SetEditorPattern(CPattern *p)
{
	pmi->pEditorPattern = p != NULL ? pmi->patterns.find(p)->second.get() : NULL;
	pmi->gui.SetEditorPattern(pmi->pEditorPattern);
}

void miex::SetPatternTargetMachine(CPattern *p, CMachine *pmac)
{
	pmi->TargetMac = pmac;
	pmi->gui.SetTargetMachine(pmac);
}

bool miex::ShowPatternEditorHelp()
{
	pmi->gui.ShowHelp();
	return true;
}

void miex::SetBaseOctave(int bo)
{
	pmi->gui.SetBaseOctave(bo);
}

int miex::GetEditorPatternPosition()
{
	if (pmi->pEditorPattern == NULL)
		return 0;

	return pmi->gui.GetEditorPatternPosition();
}

void miex::PlayPattern(CPattern *p, CSequence *s, int offset)
{
	if (pmi->pPlayingPattern != NULL)
	{
		pmi->posInTick = 0;
		pmi->patternPos++;
	}

	pmi->StopPlayingNotes();

	pmi->pPlayingPattern = p != NULL ? pmi->patterns[p].get() : NULL;
	pmi->patternPos = offset - 1;
}

bool miex::EnableCommandUI(int id)
{
	switch(id)
	{
	case ID_EDIT_UNDO: return pmi->pEditorPattern != NULL && pmi->pEditorPattern->actionStack.CanUndo(); break;
	case ID_EDIT_REDO: return pmi->pEditorPattern != NULL && pmi->pEditorPattern->actionStack.CanRedo(); break;
	}
	return false;
}


void mi::MidiNote(int const channel, int const value, int const velocity)
{
	if (TargetMac == NULL)
		return;

	::EnterCriticalSection(&amnCS);

	pCB->SendMidiNote(TargetMac, channel, value, velocity);

	if (velocity > 0)
		activeMidiNotes.insert((channel << 16) | value);
	else
		activeMidiNotes.erase((channel << 16) | value);
			
	AddToRecBuffer(value, velocity);

	if (gui.Window != NULL && channel == 0)
	{
		::EnterCriticalSection(&nd.PatternCS);
		nd.notesToGUI.push_back(NoteToGUI(MTEvent(value, velocity, 0), true));
		::LeaveCriticalSection(&nd.PatternCS);
	}

	::LeaveCriticalSection(&amnCS);

}

void miex::MidiControlChange(int const ctrl, int const channel, int const value)
{
	if (pmi->TargetMac == NULL)
		return;

	pmi->pCB->SendMidiControlChange(pmi->TargetMac, ctrl, channel, value);
}

void mi::GotMidiFocus()
{
	if (TargetMac == NULL) return;
}

void mi::LostMidiFocus()
{
	if (TargetMac == NULL) return;

	::EnterCriticalSection(&amnCS);

	for (set<int>::iterator i = activeMidiNotes.begin(); i != activeMidiNotes.end(); i++)
	{
		pCB->SendMidiNote(TargetMac, *i >> 16, *i & 0xffff, 0);

		if (gui.Window != NULL && (*i >> 16) == 0)
		{
			::EnterCriticalSection(&nd.PatternCS);
			nd.notesToGUI.push_back(NoteToGUI(MTEvent(*i & 0xffff, 0, 0), true));
			::LeaveCriticalSection(&nd.PatternCS);
		}

	}
	
		// NOTE: could also send All Notes Off here

	activeMidiNotes.clear();

	::LeaveCriticalSection(&amnCS);

}

void mi::AddToRecBuffer(int key, int vel)
{
	if (pCB->GetStateFlags() & SF_RECORDING)
	{
		if (!recording && pEditorPattern != NULL)
			pEditorPattern->actionStack.BeginAction(pEditorPattern, "Record");

		::EnterCriticalSection(&nd.PatternCS);

		recording = true;

		if (vel > 0)
			noteOnRecBuffer.push_back(MTEvent(key, vel, 0));
		else
			noteOffRecBuffer.push_back(MTEvent(key, vel, 0));

		::LeaveCriticalSection(&nd.PatternCS);
	}
}


void mi::Stop()
{
	StopPlayingNotes();
	nd.playingNoteSet->Stop();

	recording = false;

	pPlayingPattern = NULL;
	patternPos = -1;
	nd.PlayPos = -1;
}


void mi::Tick()
{
	if (pPlayingPattern != NULL)
	{
		posInTick = 0;
		patternPos++;

		if (pEditorPattern == pPlayingPattern)
			nd.PlayPos = patternPos * (960 / 4);
		else
			nd.PlayPos = -1;

		if (patternPos >= pPlayingPattern->length)
		{
			StopPlayingNotes();

			patternPos = -1;
			pPlayingPattern = NULL;
			nd.PlayPos = -1;
		}


	}
}

void mi::StopPlayingNotes()
{
	vector<MTEvent> e;
	
	::EnterCriticalSection(&nd.PatternCS);

	for (MapIntToMTEvent::const_iterator i = playingNotes.begin(); i != playingNotes.end(); i++)
	{
		e.push_back((*i).second);
		nd.notesToGUI.push_back(NoteToGUI((*i).second.Off(), false));
	}

	playingNotes.clear();

	noteOnRecBuffer.clear();
	noteOffRecBuffer.clear();

	if (pPlayingPattern != NULL)
	{
		CMachineTrack &mt = *pPlayingPattern->tracks[0].get();
		int t = patternPos * (PIANOROLL_TPB / 4);

		for (map<int, MTEvent>::iterator i = recordingNotes.begin(); i != recordingNotes.end(); i++)
		{
			int len = t - (*i).second.Length;
			if (len > 0)
			{
				mt.events[pair<int, int>((*i).second.Length, (*i).second.D1)] = MTEvent((*i).second.D1, (*i).second.D2, len);
				nd.recNotesToGUI.push_back(pair<int, MTEvent>((*i).second.Length, MTEvent((*i).second.D1, (*i).second.D2, len)));
			}

		}
	}

	recordingNotes.clear();

	::LeaveCriticalSection(&nd.PatternCS);

	for (int i = 0; i < (int)e.size(); i++)
		pCB->SendMidiNote(TargetMac, 0, e[i].D1, 0);
}

bool mi::Work(float *pout, int numsamples, int const mode)
{
	CSubTickInfo const *psti = pCB->GetSubTickInfo();
	if (psti != NULL && psti->PosInSubTick == 0 && pPlayingPattern != NULL)
	{
		int t = patternPos * (PIANOROLL_TPB / 4) + psti->CurrentSubTick * (PIANOROLL_TPB / 4) / psti->SubTicksPerTick;
		int tn = t + (PIANOROLL_TPB / 4) / psti->SubTicksPerTick;

		//if (pEditorPattern == pPlayingPattern)
			//nd.PlayPos = t;

		if (pPlayingPattern->tracks.size() > 0)
		{
			CMachineTrack &mt = *pPlayingPattern->tracks[0].get();

			vector<MTEvent> e;

			for (MapIntToMTEvent::iterator i = playingNotes.lower_bound(pair<int, int>(t, 0)); i != playingNotes.end() && (*i).first < pair<int, int>(tn, 0); i++)
				pCB->SendMidiNote(TargetMac, 0, (*i).second.D1, 0);

			::EnterCriticalSection(&nd.PatternCS);

			for (MapIntToMTEvent::iterator i = playingNotes.lower_bound(pair<int, int>(t, 0)); i != playingNotes.end() && (*i).first < pair<int, int>(tn, 0);)
			{
				MapIntToMTEvent::iterator t = i++;
				nd.notesToGUI.push_back(NoteToGUI((*t).second.Off(), false));
				playingNotes.erase(t);
			}

			for (MapIntToMTEvent::const_iterator i = mt.events.lower_bound(pair<int, int>(t, 0)); i != mt.events.end() && (*i).first < pair<int, int>(tn, 0); i++)
			{
				int endt = (*i).first.first + (*i).second.Length;
				if (endt >= tn)
				{
					e.push_back((*i).second);
					playingNotes[pair<int, int>(endt, (*i).first.second)] = (*i).second;
					nd.notesToGUI.push_back(NoteToGUI((*i).second, false));
				}
			}

			if (noteOffRecBuffer.size() > 0)
			{
				for (vector<MTEvent>::iterator i = noteOffRecBuffer.begin(); i != noteOffRecBuffer.end(); i++)
				{
					map<int, MTEvent>::iterator j = recordingNotes.find((*i).D1);
					if (j != recordingNotes.end())
					{
						int len = t - (*j).second.Length;
						len = max(len, tn - t);	// <---- maybe not a good idea
						if (len > 0)
						{
							mt.events[pair<int, int>((*j).second.Length, (*j).second.D1)] = MTEvent((*j).second.D1, (*j).second.D2, len);
							nd.recNotesToGUI.push_back(pair<int, MTEvent>((*j).second.Length, MTEvent((*j).second.D1, (*j).second.D2, len)));
						}

						recordingNotes.erase(j);
					}

				}

				noteOffRecBuffer.clear();
			}

			if (noteOnRecBuffer.size() > 0)
			{
				for (vector<MTEvent>::iterator i = noteOnRecBuffer.begin(); i != noteOnRecBuffer.end(); i++)
					recordingNotes[(*i).D1] = MTEvent((*i).D1, (*i).D2, t);	// using length to store time

				noteOnRecBuffer.clear();
			}

			::LeaveCriticalSection(&nd.PatternCS);
		
			for (int i = 0; i < (int)e.size(); i++)
				pCB->SendMidiNote(TargetMac, 0, e[i].D1, e[i].D2);

		}
	}

	if (psti != NULL && psti->PosInSubTick == 0)
		nd.playingNoteSet->Process(PIANOROLL_TPB / 4 / psti->SubTicksPerTick);

	return false;

}
