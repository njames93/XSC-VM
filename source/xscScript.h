#pragma once
#include <inttypes.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include "keyboard.h"
#include <map>

class xscHeader
{
public:
	xscHeader(std::ifstream& file);
	bool fillNative(PUINT64 Target);
	void fillCode(unsigned char *Target);
	void fillStrings(unsigned char*Target);
	void fillStatics(PUINT64 Taget);
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
private:


	int rsc7Off;
	std::ifstream& _file;
	unsigned char* buffer;
};

class xscScript
{
public:
	xscScript(std::string file, std::string Name);
	~xscScript();
	bool isRunning;
	void Terminate();
	bool OnTick();
	void IntToString(int value, unsigned char* ptr);
	std::string ExitReason;
	int maxcodesize;
private:
	void BreakPoint(bool pause_execution);
	PUINT64 _stackPointer;
	PUINT64 _stack;
	PUINT64 _locals;
	unsigned char* buffer1;
	unsigned char* buffer2;
	unsigned char* debuglist;
	unsigned char* _strings;
	unsigned char* _codeTable;
	int _codeTableIndex;
	DWORD waituntil;
	PUINT64 _framePointer;
	PUINT64 NativeHashes;
	std::string _Name;
	
};

