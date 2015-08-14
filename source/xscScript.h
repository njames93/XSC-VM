#pragma once
#include <inttypes.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include "keyboard.h"
#include <map>
#include <vector>


class script
{
public:
	script(std::string file, std::string Name);
	script();
	~script();
	bool Load();
	void Unload();
	bool fillNative(PUINT64 Target);
	void fillCode(unsigned char *Target);
	void fillStrings(unsigned char*Target);
	void fillLocals(PUINT64 Taget);
	UINT readuint(int position);
	UINT readuintptr(int position);
	int codeSize;
	int staticSize;
	int stringSize;
	int nativeSize;
	int codeOff;
	int staticOff;
	int stringsOff;
	int nativeOff;


	std::string ExitReason;

	bool isLoaded = false;
	PUINT64 _locals;

	unsigned char* _strings;
	unsigned char* _codeTable;

	PUINT64 NativeHashes;
	std::string _Name;
	int instances = 0;
	bool markedForUnload;
private:

	int rsc7Off;
	unsigned char* buffer;



	std::ifstream _file;
	std::string _filename;
};

class runningScript
{
public:
	runningScript(script& Base);
	runningScript();
	runningScript(runningScript& other);
	~runningScript();
	bool isRunning;
	void Terminate();
	bool Run();
	void IntToString(int value, unsigned char* ptr);
	std::string ExitReason;
private:
#if !NDEBUG
	char* logbyte = new char[4];
#endif
	PUINT64 _locals;
	void BreakPoint(bool pause_execution);
	PUINT64 _stackPointer;
	PUINT64 _stack;
	unsigned char* buffer1;
	unsigned char* buffer2;
	unsigned char* debuglist;
	int _codeTableIndex;
	DWORD waituntil;
	PUINT64 _framePointer;
	script& _base;
};

class runningScripts
{
public:
	~runningScripts();
	void Run();
	void StartNewScript(script& Script);
	std::vector<runningScript> scripts;
private:
};
