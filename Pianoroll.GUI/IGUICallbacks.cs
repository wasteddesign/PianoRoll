using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Pianoroll.GUI
{
    public interface IGUICallbacks
    {
        void WriteDC(string text);
        int GetPlayPosition();
        bool GetPlayNotesState();
        void MidiNote(int note, int velocity);
        int GetBaseOctave();
        bool IsMidiNoteImplemented();
        void GetGUINotes();
        void GetRecordedNotes();
        int GetStateFlags();
        bool TargetSet();
	    void SetStatusBarText(int pane, String text);
        void PlayNoteEvents(IEnumerable<NoteEvent> notes);
        bool IsEditorWindowVisible();
		string GetThemePath();

    }
}
