#pragma once

class CMachinePattern;

typedef vector<byte> ByteVector;

class CState
{
public:
	string name;
	ByteVector state;
	int patternLength;
};

typedef vector<shared_ptr<CState>> StateVector;

class CEditorWnd;

class CActionStack
{
public:
	CActionStack();
	~CActionStack();

	void BeginAction(CMachinePattern *pmp, char const *name);

	void Undo(CMachinePattern *pmp);
	void Redo(CMachinePattern *pmp);

	bool CanUndo() const { return position > 0; }
	bool CanRedo() const { return position < (int)states.size(); }

private:
	void RestoreState(CMachinePattern *pmp);
	void SaveState(CMachinePattern *pmp, CState &s);

private:
	int position;
	StateVector states;
	CState unmodifiedState;
	CRITICAL_SECTION cs;

};
