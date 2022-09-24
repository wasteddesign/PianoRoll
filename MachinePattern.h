#pragma once

#include "ActionStack.h"

using namespace System::Collections::Generic;
using namespace Pianoroll::GUI;

#pragma unmanaged

struct MTEvent
{
	MTEvent() {}
	MTEvent(int d1, int d2, int l)
	{
		D1 = d1;
		D2 = d2;
		Length = l;
	}

	MTEvent Off() const
	{
		MTEvent x;
		x.D1 = D1;
		x.D2 = 0;
		x.Length = Length;
		return x;
	}

	int D1;
	int D2;
	int Length;
};

typedef map<pair<int, int>, MTEvent> MapIntToMTEvent;

struct NoteToGUI
{
	NoteToGUI(MTEvent const &e, bool fromuser) 
	{
		this->e = e;
		FromUser = fromuser;
	}

	MTEvent e;
	bool FromUser;
};

typedef void (CMachineInterface::*RecordCallback)(int key, int vel);
class CPlayingNoteSet;

struct NativeData
{
	CMachineInterface *pMI;
	CMICallbacks *pCB;
	CMachine *TargetMac;
	int PlayPos;
	CRITICAL_SECTION PatternCS;
	vector<NoteToGUI> notesToGUI;
	vector<pair<int, MTEvent>> recNotesToGUI;
	RecordCallback recordCallback;
	shared_ptr<CPlayingNoteSet> playingNoteSet;
};

class CPlayingNoteSet
{
public:
	CPlayingNoteSet(NativeData *pnd)
	{
		this->pnd = pnd;
	}

	void Play(MapIntToMTEvent const &e)
	{
		if (e.size() == 0)
			return;

		Stop();

		::EnterCriticalSection(&pnd->PatternCS);

		events = e;
		position = events.begin()->first.first;
		playingNotes.clear();
		noMoreNotes = false;

		::LeaveCriticalSection(&pnd->PatternCS);
	}

	void Stop()
	{
		::EnterCriticalSection(&pnd->PatternCS);
		
		events.clear();

		if (playingNotes.size() == 0)
		{
			::LeaveCriticalSection(&pnd->PatternCS);
			return;
		}

		vector<MTEvent> e;

		for (MapIntToMTEvent::const_iterator i = playingNotes.begin(); i != playingNotes.end(); i++)
		{
			e.push_back((*i).second);
			pnd->notesToGUI.push_back(NoteToGUI((*i).second.Off(), false));
		}

		playingNotes.clear();

		::LeaveCriticalSection(&pnd->PatternCS);

		for (int i = 0; i < (int)e.size(); i++)
			pnd->pCB->SendMidiNote(pnd->TargetMac, 0, e[i].D1, 0);

	}

	void Process(int nticks)
	{
		::EnterCriticalSection(&pnd->PatternCS);

		if (events.size() == 0)
		{
			::LeaveCriticalSection(&pnd->PatternCS);
			return;
		}

		vector<MTEvent> ons;
		vector<MTEvent> offs;

		int t = position;
		int tn = t + nticks;

		for (MapIntToMTEvent::iterator i = playingNotes.lower_bound(pair<int, int>(t, 0)); i != playingNotes.end() && (*i).first < pair<int, int>(tn, 0);)
		{
			MapIntToMTEvent::iterator t = i++;
			offs.push_back((*t).second);
			pnd->notesToGUI.push_back(NoteToGUI((*t).second.Off(), false));
			playingNotes.erase(t);
		}

		MapIntToMTEvent::iterator last = --events.end();

		for (MapIntToMTEvent::const_iterator i = events.lower_bound(pair<int, int>(t, 0)); i != events.end() && (*i).first < pair<int, int>(tn, 0); i++)
		{
			if (i == last)
				noMoreNotes = true;

			int endt = (*i).first.first + (*i).second.Length;
			if (endt >= tn)
			{
				ons.push_back((*i).second);
				playingNotes[pair<int, int>(endt, (*i).first.second)] = (*i).second;
				pnd->notesToGUI.push_back(NoteToGUI((*i).second, false));
			}
		}

		if (noMoreNotes && playingNotes.empty())
			events.clear();
		else
			position += nticks;

		::LeaveCriticalSection(&pnd->PatternCS);

		for (int i = 0; i < (int)offs.size(); i++)
			pnd->pCB->SendMidiNote(pnd->TargetMac, 0, offs[i].D1, 0);

		for (int i = 0; i < (int)ons.size(); i++)
			pnd->pCB->SendMidiNote(pnd->TargetMac, 0, ons[i].D1, ons[i].D2);

	}

private:
	int position;
	MapIntToMTEvent events;
	MapIntToMTEvent playingNotes;
	NativeData *pnd;
	bool noMoreNotes;
};




inline void WriteVL7(dword x, CMachineDataOutput * const po)
{
	byte y;

	do
	{
		y = x & 0x7f;
		x >>= 7;
		if (x) y |= 0x80;
		po->Write(y);
	} while(x);

}

inline bool ReadVL7(dword &x, CMachineDataInput * const pi)
{
	byte y;
	int s = 0;
	x = 0;

	do
	{
		pi->Read(y);
		if (s >= 28 && (y >> 4)) return false;
		x |= (y & 0x7f) << s;
		s += 7;
	} while(y & 0x80);
	
	return true;
}

inline int EncodeMidi(int status, int data1, int data2) { return (int)(status | (data1 << 8) | (data2 << 16)); }
inline int EncodeMidiNoteOn(int channel, int note, int velocity) { return EncodeMidi(0x90 + channel, note, velocity); }
inline int EncodeMidiNoteOff(int channel, int note) { return EncodeMidi(0x80 + channel, note, 0); }

class CMachineTrack
{
public:
	CMachineTrack()
	{
	}

	CMachineTrack(CMachineTrack const &x)
	{
		events = x.events;
	}

	void AddNote(int t, MTEvent const &note)
	{
		events[pair<int, int>(t, note.D1)] = note;
//		events[pair<int, int>(note.Time, note.Note)] = MTEvent(note.Note, note.Velocity, note.Length);
	}

	void Write(CMachineDataOutput * const po)
	{
		po->Write((int)events.size());
		int lt = 0;

		for (MapIntToMTEvent::iterator i = events.begin(); i != events.end(); i++)
		{
			int t = (*i).first.first;
			int dt = t - lt;

			WriteVL7((dword)dt, po);
			WriteVL7((dword)(*i).second.Length, po);
			WriteVL7((dword)(*i).second.D1, po);
			WriteVL7((dword)(*i).second.D2, po);

			lt = t;
		}
	}

	void Read(CMachineDataInput * const pi)
	{
		int ec;
		pi->Read(ec);
		
		int t = 0;
		
		for (int i = 0; i < ec; i++)
		{
			dword dt, l, d1, d2;
			
			ReadVL7(dt, pi);
			ReadVL7(l, pi);
			ReadVL7(d1, pi);
			ReadVL7(d2, pi);
			
			t += dt;
			events[pair<int, int>(t, (int)d1)] = MTEvent((int)d1, (int)d2, (int)l);
		}

	}

	void Trim(int len)
	{
		for (MapIntToMTEvent::iterator i = events.begin(); i != events.end();)
		{
			MapIntToMTEvent::iterator t = i++;
			if ((*t).first.first >= len)
				events.erase(t);
			else if ((*t).first.first + (*t).second.Length > len)
				(*t).second.Length = len - (*t).first.first;
		}
	}

	void ImportMidiEvents(CMachineDataInput * const pi, int length)
	{
		events.clear();

		MapIntToMTEvent active;

		int lasttime = -1;

		while(true)
		{
			int time;
			pi->Read(time);
			if (time < 0) break;
			assert(time >= lasttime);
			lasttime = time;

			time /= 4;

			int mididata;
			pi->Read(mididata);

			if (time >= length) continue;

			int status = mididata & 0xff;
			int data1 = (mididata >> 8) & 0xff;
			int data2 = (mididata >> 16) & 0xff;

			if (status == 0x90)
			{
				active[pair<int, int>(time, data1)] = MTEvent(data1, data2, 0);
			}
			else if (status == 0x80)
			{
				for (auto i = active.begin(); i != active.end(); i++)
				{
					if ((*i).first.second == data1)
					{
						(*i).second.Length = time - (*i).first.first;
						events[(*i).first] = (*i).second;
						active.erase(i);
						break;
					}
				}
			}

		}

		for (auto i = active.begin(); i != active.end(); i++)
		{
			(*i).second.Length = length - (*i).first.first;
			if ((*i).second.Length > 0) events[(*i).first] = (*i).second;
		}

		
	}

	bool ExportMidiEvents(CMachineDataOutput *pout, int length)
	{
		if (events.size() == 0) return false;

		MapIntToMTEvent temp;

		for (auto i = events.begin(); i != events.end(); i++)
		{
			temp[(*i).first] = (*i).second;
			temp[pair<int, int>((*i).first.first + (*i).second.Length, -((*i).first.second + 1))] = MTEvent((*i).second.D1, (*i).second.D2, 0);
		}

		for (auto i = temp.begin(); i != temp.end(); i++)
		{
			pout->Write((*i).first.first * 4);
			if ((*i).first.second >= 0)
				pout->Write(EncodeMidiNoteOn(0, (*i).second.D1, (*i).second.D2));
			else
				pout->Write(EncodeMidiNoteOff(0, (*i).second.D1));

		}
		
		pout->Write(-1);
		return true;
	}


public:
	MapIntToMTEvent events;
};

#pragma managed

class CMachinePattern;

ref class NativePattern : public INativePattern
{
public:
	NativePattern(CMachinePattern *p)
	{
		this->p = p;
	}

    virtual void AddTrack();
	virtual void AddNote(int track, NoteEvent note);
	virtual void DeleteNotes(int track, IEnumerable<NoteEvent> ^notes);
	virtual void MoveNotes(int track, IEnumerable<NoteEvent> ^notes, int dx, int dy);
	virtual void SetVelocity(int track, IEnumerable<NoteEvent> ^notes, int velocity);
	virtual void SetLength(int track, IEnumerable<NoteEvent> ^notes, int length);
	virtual List<NoteEvent> ^GetAllNotes(int track);
	virtual NoteEvent GetAdjacentNote(int track, NoteEvent note, bool next);
	virtual void BeginAction(System::String ^name);
	virtual void SetHostLength(int nbuzzticks);

private:
	CMachinePattern *p;
};



class CMachinePattern
{
public:
	CMachinePattern(NativeData *pnd)
	{
		this->pnd = pnd;
		pPattern = NULL;
	}

	CMachinePattern(CPattern *p, int numrows, NativeData *pnd)
	{
		this->pnd = pnd;
		Init(p, numrows, pnd);
	}

	CMachinePattern(CPattern *p, CMachinePattern *pold, NativeData *pnd)
	{
		this->pnd = pnd;
		pPattern = p;
		length = pold->length;

		for (int i = 0; i < (int)pold->tracks.size(); i++)
			tracks.push_back(shared_ptr<CMachineTrack>(new CMachineTrack(*pold->tracks[i].get())));

		MPattern = gcnew Pattern(gcnew NativePattern(this), length);
	}

	void Init(CPattern *p, int numrows, NativeData *pnd)
	{
		this->pnd = pnd;
		length = numrows;
		pPattern = p;
		MPattern = gcnew Pattern(gcnew NativePattern(this), numrows);
	}
	
	void SetLength(int l)
	{
		length = l;

		::EnterCriticalSection(&pnd->PatternCS);

		for (int i = 0; i < (int)tracks.size(); i++)
			tracks[i]->Trim((length / 4) * 960);

		::LeaveCriticalSection(&pnd->PatternCS);

		MPattern->LengthInBuzzTicks = l;
	}

	void Write(CMachineDataOutput * const po)
	{
		po->Write((int)tracks.size());
		for (int i = 0; i < (int)tracks.size(); i++)
			tracks[i]->Write(po);
	}

	void Read(CMachineDataInput * const pi)
	{
		::EnterCriticalSection(&pnd->PatternCS);

		tracks.clear();

		int tc;
		pi->Read(tc);
		for (int i = 0; i < tc; i++)
		{
			tracks.push_back(shared_ptr<CMachineTrack>(new CMachineTrack()));
			tracks.back()->Read(pi);
		}

		::LeaveCriticalSection(&pnd->PatternCS);
	}

	bool ImportMidiEvents(CMachineDataInput * const pin)
	{
		actionStack.BeginAction(this, "Import MIDI Events");

		::EnterCriticalSection(&pnd->PatternCS);

		tracks[0]->ImportMidiEvents(pin, length / 4 * 960);

		::LeaveCriticalSection(&pnd->PatternCS);

		MPattern->Update();

		return true;
	}

	bool ExportMidiEvents(CMachineDataOutput * const pout)
	{
		return tracks[0]->ExportMidiEvents(pout, length / 4 * 960);
	}

public:
	CPattern *pPattern;
	int length;
	vector<shared_ptr<CMachineTrack>> tracks;
	NativeData *pnd;
	CActionStack actionStack;
	gcroot<Pattern ^> MPattern;

};

typedef map<CPattern *, shared_ptr<CMachinePattern>> MapPatternToMachinePattern;
typedef map<string, shared_ptr<CMachinePattern>> MapStringToMachinePattern;


inline void NativePattern::AddTrack()
{
	::EnterCriticalSection(&p->pnd->PatternCS);
	p->tracks.push_back(shared_ptr<CMachineTrack>(new CMachineTrack()));
	::LeaveCriticalSection(&p->pnd->PatternCS);
}

inline void NativePattern::AddNote(int track, NoteEvent note)
{
	::EnterCriticalSection(&p->pnd->PatternCS);
	p->tracks[track]->AddNote(note.Time, MTEvent(note.Note, note.Velocity, note.Length));
	::LeaveCriticalSection(&p->pnd->PatternCS);
}


inline void NativePattern::DeleteNotes(int track, IEnumerable<NoteEvent> ^notes)
{
	if (notes == nullptr)
	{
		::EnterCriticalSection(&p->pnd->PatternCS);
		p->tracks[track]->events.clear();
		::LeaveCriticalSection(&p->pnd->PatternCS);
		return;
	}

	vector<pair<int, int>> n;

	for each (NoteEvent e in notes)
		n.push_back(pair<int, int>(e.Time, e.Note));

	MapIntToMTEvent &em = p->tracks[track]->events;

	::EnterCriticalSection(&p->pnd->PatternCS);
	for (vector<pair<int, int>>::iterator i = n.begin(); i != n.end(); i++)
	{
		MapIntToMTEvent::iterator j = em.find(*i);
		if (j != em.end())
			em.erase(j);
	}
	::LeaveCriticalSection(&p->pnd->PatternCS);

}

inline void NativePattern::MoveNotes(int track, IEnumerable<NoteEvent> ^notes, int dx, int dy)
{
	MapIntToMTEvent n;

	for each (NoteEvent e in notes)
		n[pair<int, int>(e.Time, e.Note)] = MTEvent(e.Note, e.Velocity, e.Length);

	MapIntToMTEvent &em = p->tracks[track]->events;

	::EnterCriticalSection(&p->pnd->PatternCS);

	for (auto i = n.begin(); i != n.end(); i++)
	{
		MapIntToMTEvent::iterator j = em.find((*i).first);
		if (j != em.end())
			em.erase(j);
	}

	for (auto i = n.begin(); i != n.end(); i++)
	{
		em[pair<int, int>((*i).first.first + dy, (*i).first.second + dx)] = MTEvent((*i).second.D1 + dx, (*i).second.D2, (*i).second.Length);
	}

	::LeaveCriticalSection(&p->pnd->PatternCS);

}

inline void NativePattern::SetVelocity(int track, IEnumerable<NoteEvent> ^notes, int velocity)
{
	vector<pair<int, int>> n;

	for each (NoteEvent e in notes)
		n.push_back(pair<int, int>(e.Time, e.Note));

	MapIntToMTEvent &em = p->tracks[track]->events;

	::EnterCriticalSection(&p->pnd->PatternCS);
	for (vector<pair<int, int>>::iterator i = n.begin(); i != n.end(); i++)
	{
		MapIntToMTEvent::iterator j = em.find(*i);
		if (j != em.end())
			(*j).second.D2 = velocity;
	}
	::LeaveCriticalSection(&p->pnd->PatternCS);
}

inline void NativePattern::SetLength(int track, IEnumerable<NoteEvent> ^notes, int length)
{
	vector<pair<int, int>> n;

	for each (NoteEvent e in notes)
		n.push_back(pair<int, int>(e.Time, e.Note));

	MapIntToMTEvent &em = p->tracks[track]->events;

	::EnterCriticalSection(&p->pnd->PatternCS);
	for (vector<pair<int, int>>::iterator i = n.begin(); i != n.end(); i++)
	{
		MapIntToMTEvent::iterator j = em.find(*i);
		if (j != em.end())
			(*j).second.Length = length;
	}
	::LeaveCriticalSection(&p->pnd->PatternCS);
}

inline List<NoteEvent> ^NativePattern::GetAllNotes(int track)
{
	List<NoteEvent> ^notes = gcnew List<NoteEvent>();

	vector<pair<int, MTEvent>> e;

	::EnterCriticalSection(&p->pnd->PatternCS);
	for (MapIntToMTEvent::iterator i = p->tracks[track]->events.begin(); i != p->tracks[track]->events.end(); i++)
		e.push_back(pair<int, MTEvent>((*i).first.first, (*i).second));
	::LeaveCriticalSection(&p->pnd->PatternCS);

	for (int i = 0; i < (int)e.size(); i++)
		notes->Add(NoteEvent(e[i].first, e[i].second.Length, e[i].second.D1, e[i].second.D2));

	return notes;
}

inline NoteEvent NativePattern::GetAdjacentNote(int track, NoteEvent note, bool next)
{
	MapIntToMTEvent &em = p->tracks[track]->events;
	MapIntToMTEvent::iterator j;
	
	if (note.IsInvalid)
	{
		if (next)
		{
			j = em.end();
			if (em.size() > 0)
				j--;
		}
		else
		{
			j = em.begin();
		}
	}
	else
		j = em.find(pair<int, int>(note.Time, note.Note));

	if (j != em.end())
	{
		if (next)
		{
			j++;
			if (j == em.end())
				j--;
		}
		else
		{
			if (j != em.begin())
				j--;
		}

		return NoteEvent((*j).first.first, (*j).second.Length, (*j).second.D1, (*j).second.D2);
	}
	else
	{
		return NoteEvent::Invalid;
	}

}

inline void NativePattern::BeginAction(System::String ^name)
{
	string n = msclr::interop::marshal_as<string>(name);
	p->actionStack.BeginAction(p, n.c_str());
}


inline void NativePattern::SetHostLength(int nbuzzticks)
{
	p->pnd->pCB->SetPatternLength(p->pPattern, nbuzzticks);
}
