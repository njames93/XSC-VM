#include "xscScript.h"
#include "main.h"
#include "NativeTranslation.h"
#include "script.h"
#include <sstream>
#include "natives.h"

std::string HexToString(unsigned int hex)
{
	std::stringstream sstream;
	sstream << std::hex << hex;
	return "0x" + sstream.str();
}

std::string LongHexToString(unsigned long long hex)
{
	std::stringstream sstream;
	sstream << std::hex << hex;
	return "0x" + sstream.str();
}

UINT script::readuint(int position)
{
	_file.clear();
	_file.seekg(position, std::ios_base::beg);
	_file.read((char *)buffer, 4);
	UINT ret = buffer[0] << 24;
	ret |= buffer[1] << 16;
	ret |= buffer[2] << 8;
	ret |= buffer[3];
	return ret;
}
UINT script::readuintptr(int position)
{
	_file.clear();
	_file.seekg(position, std::ios_base::beg);
	_file.read((char *)buffer, 4);
	UINT ret = buffer[1] << 16;
	ret |= buffer[2] << 8;
	ret |= buffer[3];
	return ret;
}
bool script::fillNative(PUINT64 Target)
{
	UINT hash;
	_file.clear();
	_file.seekg(nativeOff, std::ios_base::beg);
	for (int i = 0; i < nativeSize; i++)
	{
		_file.read((char*)buffer , 4);
		hash = buffer[0] << 24;
		hash |= buffer[1] << 16;
		hash |= buffer[2] << 8;
		hash |= buffer[3];
		if (NativeTranslation::Exists(hash))
			*(Target++) = NativeTranslation::GetNative(hash);
		else
		{
			ExitReason = "Error with native " + HexToString(hash) + ", Native read from " + HexToString(nativeOff) + " + " + HexToString(4 * i);
			return false;
		}
	}
	return true;
}

void script::fillCode(unsigned char* Target)
{
	int pagecount = (codeSize + 0x3fff) >> 14;
	for (int i = 0; i < pagecount;i++)
	{
		_file.clear();
		UINT pagestart = readuintptr(codeOff + (i << 2)) + rsc7Off;
		int pagesize = ((i + 1) * 0x4000 >= codeSize) ? codeSize % 0x4000 : 0x4000;
		//Log("Read codepage " + std::to_string(i) + " from " + HexToString(pagestart));
		_file.seekg(pagestart, std::ios_base::beg);
		_file.read((char *)Target, pagesize);
		Target += pagesize;
	}
}

void script::fillStrings(unsigned char* Target)
{
	int pagecount = (stringSize + 0x3fff) >> 14;
	for (int i = 0; i < pagecount; i++)
	{
		_file.clear();
		UINT pagestart = readuintptr(stringsOff + (i << 2)) + rsc7Off;
		int pagesize = ((i + 1) * 0x4000 >= stringSize) ? stringSize % 0x4000 : 0x4000;
		//Log("Read stringpage " + std::to_string(i) + " from " + HexToString(pagestart));
		_file.seekg(pagestart, std::ios_base::beg);
		_file.read((char *)Target, pagesize);
		Target += pagesize;
	}
}

void script::fillLocals(PUINT64 Target)
{
	PUINT64 temp = Target;
	for (int i = 0; i < staticSize; i++)
	{
		*temp = 0;
		temp++;
	}
	UINT hash;
	_file.clear();
	_file.seekg(staticOff, _file.beg);
	for (int i = 0; i < staticSize; i++)
	{
		_file.read((char*)buffer, 4);
		hash = buffer[0] << 24;
		hash |= buffer[1] << 16;
		hash |= buffer[2] << 8;
		hash |= buffer[3];
		*Target++ = (UINT64)hash;
	}
}

void runningScript::BreakPoint(bool pause_execution)
{
	std::string Message = _base._Name + ": Breakpoint hit at position " + HexToString(_codeTableIndex+1/*Dont need to display throw opcode*/) + ", Next opcode = " + HexToString(_base._codeTable[_codeTableIndex + 1]) + "\n";
	Message += "Function call address = " + HexToString(*(int*)(_framePointer + *(debuglist - 1)))+"\n\n";
	PUINT64 stackstart = _framePointer + *debuglist + *(debuglist - 1) - 1;
	Message += (*(debuglist - 1) > 0 ? "Function parameters are:\n" : "No function parameters\n");
	for (int i = 0; i < *(debuglist - 1); i++)
	{
		Message += "32bit = " + HexToString(*(int*)(_framePointer + i))+ ", 64bit = " + LongHexToString(*(_framePointer + i)) + "\n";
	}
	Message += "\n" + (*debuglist > 2) ? "Function variables are:\n" : "Function has no variables";
	for (int i = 2; i < *debuglist; i++)
	{
		i += *(debuglist - 1);
		Message += "32bit = " + HexToString(*(int*)(_framePointer + i)) + ", 64bit = " + LongHexToString(*(_framePointer + i)) + "\n";
	}
	if (stackstart == _stack)
	{
		Message += "\nStack is empty";
	}
	else
	{
		Message += "\nStack items are:";
		int count =(int)( _stack - stackstart);
		for (int i = 0; i < count;i++)
		{
			Message += "32bit = " + HexToString(*(int*)(_stack - i)) + ", 64bit = " + LongHexToString(*(_stack-i));
		}
	}
	Log(Message);
	if (pause_execution)
	{
		while (!CONTROLS::IS_CONTROL_JUST_PRESSED(2, 237))
		{
			std::string text = "Debugpoint hit, opcode pos = " + HexToString(_codeTableIndex) + "\nPress A to Continue";
			UI::SET_TEXT_FONT(0);
			UI::SET_TEXT_SCALE(0.0f, 0.5f);
			UI::SET_TEXT_COLOUR(255, 255, 255, 255);
			UI::SET_TEXT_WRAP(0.0f, 1.0);
			UI::SET_TEXT_CENTRE(true);
			UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
			UI::SET_TEXT_EDGE(0, 0, 0, 0, 0);
			UI::_SET_TEXT_ENTRY("STRING");
			UI::_ADD_TEXT_COMPONENT_STRING((char *)text.c_str());
			UI::_DRAW_TEXT(0.5, 0.5);
			WAIT(0);
		}
		unsigned long time = 2000 + GetTickCount();
		while (time > GetTickCount())
		{
			std::string text = "resuming in " + std::to_string(time - GetTickCount()) + "ms";
			UI::SET_TEXT_FONT(0);
			UI::SET_TEXT_SCALE(0.0f, 0.5f);
			UI::SET_TEXT_COLOUR(255, 255, 255, 255);
			UI::SET_TEXT_WRAP(0.0f, 1.0);
			UI::SET_TEXT_CENTRE(true);
			UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
			UI::SET_TEXT_EDGE(0, 0, 0, 0, 0);
			UI::_SET_TEXT_ENTRY("STRING");
			UI::_ADD_TEXT_COMPONENT_STRING((char *)text.c_str());
			UI::_DRAW_TEXT(0.5, 0.5);
			WAIT(0);
		}
	}
}

script::script(std::string file, std::string Name) : _Name(Name), _filename(file)
{
	isLoaded = false;
}
script::script()
{
}
bool script::Load()
{
	markedForUnload = false;
	if (isLoaded)
		return true;
	buffer = new unsigned char[4];
	Log(_Name + ": Initialising");
	_file.open(_filename, std::ios::in | std::ios::binary | std::ios::ate);
	Log(_Name + ": opened script");
	rsc7Off = readuint(0) == 0x52534337 ? 0x10 : 0x0;
	
	codeOff = readuintptr(0x8 + rsc7Off) + rsc7Off;
	codeSize = readuint(0x10 + rsc7Off);
	staticSize = readuint(0x18 + rsc7Off);
	nativeSize = readuint(0x20 + rsc7Off);
	staticOff = readuintptr(0x24 + rsc7Off) + rsc7Off;
	nativeOff = readuintptr(0x2c + rsc7Off) + rsc7Off;
	stringsOff = readuintptr(0x44 + rsc7Off) + rsc7Off;
	stringSize = readuint(0x48 + rsc7Off);
	Log(_Name + ": CodeSize = " + std::to_string(codeSize) + ", StringSize = " + std::to_string(stringSize) + ", LocalSize = " + std::to_string(staticSize) + ", NativeSize = " + std::to_string(nativeSize));
	_locals = new UINT64[staticSize];
	_strings = new unsigned char[stringSize];
	_codeTable = new unsigned char[codeSize];
	NativeHashes = new UINT64[nativeSize];
	delete buffer;
	fillLocals(_locals);
	if (staticSize > 0)
		Log(_Name + ": Allocated statics at " + LongHexToString((UINT64)_locals));
	fillStrings(_strings);
	if (stringSize > 0)
		Log(_Name + ": Allocated strings at " + LongHexToString((UINT64)_strings));
	fillCode(_codeTable);
	Log(_Name + ": Allocated code segment at " + LongHexToString((UINT64)_codeTable));
	if (fillNative(NativeHashes))
	{

		Log(_Name + ": Allocated native table at " + LongHexToString((UINT64)NativeHashes));

		_file.close();
		isLoaded = true;
		return true;
	}
	else
	{
		delete _locals;
		Log(_Name + ": Freeing up Locals");
		delete _strings;
		Log(_Name + ": Freeing up strings");
		delete _codeTable;
		Log(_Name + ": Freeing up code table");
		delete NativeHashes;
		Log(_Name + ": Freeing up native table");
		_file.close();
		isLoaded = false;
		return false;
	}
	
}

void script::Unload()
{
	markedForUnload = true;
	if (!isLoaded)
		return;
	if (instances > 0)
		Log(_Name + " cannot be unloaded while there are still instances running");
	delete _locals;
	Log(_Name + ": Freeing up Locals");
	delete _strings;
	Log(_Name + ": Freeing up strings");
	delete _codeTable;
	Log(_Name + ": Freeing up code table");
	delete NativeHashes;
	Log(_Name + ": Freeing up native table");
	isLoaded = false;
}

void runningScript::Terminate()
{
	if (isRunning)
	{
		delete _locals;
		delete _stackPointer;
		delete buffer1;
		delete buffer2;
		delete debuglist;
#if !NDEBUG
		delete logbyte;
#endif
		isRunning = false;
		_base.instances--;
		if (_base.instances == 0)
		{
			if (_base.markedForUnload)
				_base.Unload();
		}
	}
}

bool runningScript::Run()
{
	if (!isRunning)
		return true;
	if (GetTickCount() < waituntil)
	{
		return true;
	}
	float float1, float2;
	int int1, int2, int3, int4;
	UINT64 long1, long2, long3;
	unsigned char* ptr1,* ptr2;
	short short1;


	for (;; _codeTableIndex++)
	{
		try
		{
#if !NDEBUG
			*(int*)logbyte = _codeTableIndex;
			fastlog(logbyte);
#endif
			switch (_base._codeTable[_codeTableIndex])
			{
			case 0:
				if (_codeTableIndex > _base.codeSize || _codeTableIndex < 0)//sanity check
				{
					ExitReason = "Exceeded code length, aborting";
					Terminate();
					return false;
				}//no-op
				continue;
			case 1://iadd
				_stack--;
				*(int*)_stack += *(int*)(_stack + 1);
				continue;
			case 2://isub
				_stack--;
				*(int*)_stack -= *(int*)(_stack + 1);
				continue;
			case 3://imul
				_stack--;
				*(int*)_stack *= *(int*)(_stack + 1);
				continue;
			case 4://idiv
				_stack--;
				if (*(int*)(_stack + 1) != 0)
					*(int*)_stack /= *(int*)(_stack + 1);
				continue;
			case 5://imod
				_stack--;
				if (*(int*)(_stack + 1) != 0)
					*(int*)_stack %= *(int*)(_stack + 1);
				continue;
			case 6://inot
				*(int*)_stack = (*(int*)_stack == 0) ? 1 : 0;
				continue;
			case 7://ineg
				*(int*)_stack = -*(int*)_stack;
				continue;
			case 8://icmpeq
				_stack--;
				*(int*)_stack = (*(int*)_stack == *(int*)(_stack + 1)) ? 1 : 0;
				continue;
			case 9://icmpne
				_stack--;
				*(int*)_stack = (*(int*)_stack != *(int*)(_stack + 1)) ? 1 : 0;
				continue;
			case 10://icmpgt
				_stack--;
				*(int*)_stack = (*(int*)_stack > *(int*)(_stack + 1)) ? 1 : 0;
				continue;
			case 11://icmpge
				_stack--;
				*(int*)_stack = (*(int*)_stack >= *(int*)(_stack + 1)) ? 1 : 0;
				continue;
			case 12://icmplt
				_stack--;
				*(int*)_stack = (*(int*)_stack < *(int*)(_stack + 1)) ? 1 : 0;
				continue;
			case 13://icmple
				_stack--;
				*(int*)_stack = (*(int*)_stack <= *(int*)(_stack + 1)) ? 1 : 0;
				continue;
			case 14://fadd
				_stack--;
				*(float*)_stack += *(float*)(_stack + 1);
				continue;
			case 15://fsub
				_stack--;
				*(float*)_stack -= *(float*)(_stack + 1);
				continue;
			case 16://fmul
				_stack--;
				*(float*)_stack *= *(float*)(_stack + 1);
				continue;
			case 17://fdiv
				_stack--;
				if (*(float*)(_stack + 1) != 0.0f)
					*(float*)_stack /= *(float*)(_stack + 1);
				continue;
			case 18://fmod
				_stack--;
				if (*(int*)(_stack + 1) != 0)
				{
					float1 = *(float *)_stack;
					float2 = *(float*)(_stack + 1);
					*(float*)_stack -= (float)(((int)(float1 / float2)) * float2);
				}
				continue;
			case 19://fneg
				*(int*)_stack ^= 1 << 31;
				continue;
			case 20://fcmpeq
				_stack--;
				*(int*)_stack = (*(float*)_stack == *(float*)(_stack + 1)) ? 1 : 0;
				continue;
			case 21://fcmpne
				_stack--;
				*(int*)_stack = (*(float*)_stack != *(float*)(_stack + 1)) ? 1 : 0;
				continue;
			case 22://fcmpgt
				_stack--;
				*(int*)_stack = (*(float*)_stack > *(float*)(_stack + 1)) ? 1 : 0;
				continue;
			case 23://fcmpge
				_stack--;
				*(int*)_stack = (*(float*)_stack >= *(float*)(_stack + 1)) ? 1 : 0;
				continue;
			case 24://fcmplt
				_stack--;
				*(int*)_stack = (*(float*)_stack < *(float*)(_stack + 1)) ? 1 : 0;
				continue;
			case 25://fcmple
				_stack--;
				*(int*)_stack = (*(float*)_stack <= *(float*)(_stack + 1)) ? 1 : 0;
				continue;
			case 26://vadd
				_stack -= 3;
				*(float*)_stack += *(float*)(_stack + 3);
				*(float*)(_stack - 1) += *(float*)(_stack + 2);
				*(float*)(_stack - 2) += *(float*)(_stack + 1);
				continue;
			case 27://vsub
				_stack -= 3;
				*(float*)_stack -= *(float*)(_stack + 3);
				*(float*)(_stack - 1) -= *(float*)(_stack + 2);
				*(float*)(_stack - 2) -= *(float*)(_stack + 1);
				continue;
			case 28://vmul
				_stack -= 3;
				*(float*)_stack *= *(float*)(_stack + 3);
				*(float*)(_stack - 1) *= *(float*)(_stack + 2);
				*(float*)(_stack - 2) *= *(float*)(_stack + 1);
				continue;
			case 29://vdiv
				_stack -= 3;
				if (*(int*)(_stack + 3) != 0)
					*(float*)_stack /= *(float*)(_stack + 3);
				if (*(int*)(_stack + 2) != 0)
					*(float*)(_stack - 1) /= *(float*)(_stack + 2);
				if (*(int*)(_stack + 1) != 0)
					*(float*)(_stack - 2) /= *(float*)(_stack + 1);
				continue;
			case 30://vneg
				*(int*)_stack ^= 1 << 31;
				*(int*)(_stack - 1) ^= 1 << 31;
				*(int*)(_stack - 2) ^= 1 << 31;
				continue;
			case 31://and
				_stack--;
				*(int*)_stack &= *(int*)(_stack + 1);
				continue;
			case 32://or
				_stack--;
				*(int*)_stack |= *(int*)(_stack + 1);
				continue;
			case 33://xor
				_stack--;
				*(int*)_stack ^= *(int*)(_stack + 1);
				continue;
			case 34://ItoF
				*(float*)_stack = (float)(*(int*)_stack);
				continue;
			case 35://FtoI
				*(int*)_stack = (int)(*(float*)_stack);
				continue;
			case 36://FtoV
				_stack += 2;
				*_stack = *(_stack - 2);
				*(_stack - 1) = *(_stack - 2);
				continue;
			case 37://Push1
				_stack++;
				*(int*)_stack = _base._codeTable[++_codeTableIndex];
				continue;
			case 38://Push2
				_stack += 2;
				*(int*)(_stack - 1) = _base._codeTable[++_codeTableIndex];
				*(int*)_stack = _base._codeTable[++_codeTableIndex];
				continue;
			case 39://Push3
				_stack += 3;
				*(int*)(_stack - 2) = _base._codeTable[++_codeTableIndex];
				*(int*)(_stack - 1) = _base._codeTable[++_codeTableIndex];
				*(int*)_stack = _base._codeTable[++_codeTableIndex];
				continue;
			case 40://ipush
			case 41://fpush
				_stack++;
				int1 = _base._codeTable[++_codeTableIndex] << 24;
				int1 |= _base._codeTable[++_codeTableIndex] << 16;
				int1 |= _base._codeTable[++_codeTableIndex] << 8;
				int1 |= _base._codeTable[++_codeTableIndex];
				*(int*)_stack = int1;
				continue;
			case 42://dup
				_stack++;
				*_stack = *(_stack - 1);
				continue;
			case 43://drop
				_stack--;
				continue;
			case 44://callnative
				int1 = _base._codeTable[++_codeTableIndex];//param/return byte
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];//native table index
				long1 = _base.NativeHashes[int2];//get native hash
				int3 = int1 >> 2;//get p count
				int4 = int1 & 0x3;//get r count
				long3 = (UINT64)_stack;

				_stack -= int3;//take p count from stack pointer
				switch (long1)
				{
				case 0x4EDE34FBADD967A6:
					waituntil = GetTickCount() + *(int*)(_stack + 1);//Dont use System::Wait native
					_codeTableIndex++;
					return true;
				case 0x442E0A7EDE4A738A://get this script name
					_stack++;
					*_stack = (UINT64)_base._Name.c_str();
					continue;
				case 0x1090044AD1DA76FA://terminate this thread
					Terminate();
					return true;
				case 0x8A1C8B1738FFE87E://get script name hash
					_stack++;
					nativeInit(0xD24D37CC275948CC);
					nativePush64((UINT64)_base._Name.c_str());
					*_stack = *nativeCall();
					continue;
				case 0x9243BAC96D64C050://set script safe for network game
					continue;
				}
				nativeInit(long1);
				for (int i = 0; i < int3; i++)
				{
					nativePush64(*(_stack + i + 1));//push arguments from stack to 
				}
				long2 = (UINT64)nativeCall();//call native and get return address
				for (int i = 0; i < int4; i++)//pop arguments from stack, when testing natives had no return arguments
				{
					_stack++;//incriment stack pointer
					*_stack = *(PUINT64)long2;//get native
					long2 += 8;//move to next return value(if exists)
				}
				continue;
			case 45://Function
				int1 = _base._codeTable[++_codeTableIndex];//paramcount
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];//varcount
				int3 = _base._codeTable[++_codeTableIndex];
				_codeTableIndex += int3;
				*(_stack + 2) = (UINT64)_framePointer;
				_framePointer = _stack - int1 + 1;
				_stack += int2;
				*++debuglist = int1;//for breakpoint
				*++debuglist = int2;//for breakpoint
				continue;
			case 46://Ret
				debuglist -= 2;//for breakpoint
				int1 = _base._codeTable[++_codeTableIndex];//paramcount
				int2 = _base._codeTable[++_codeTableIndex];//retcount
				_codeTableIndex = *(int*)(_framePointer + int1);
				if (_codeTableIndex == -1)
				{
					Terminate();
					return false;
				}
				long1 = *(_framePointer + int1 + 1);//old framepointer
				int3 = *(int*)_stack;
				_stack -= int2;
				_framePointer--;
				int3 = *(int*)_stack;
				for (int i = 0; i < int2; i++)
				{
					_stack++;
					_framePointer++;
					int3 = *(int*)_stack;
					int4 = *(int*)_framePointer;
					*_framePointer = *_stack;


				}
				_stack = _framePointer;
				int3 = *(int*)_stack;
				if (long1 == 0)
				{
					Terminate();
					return false;
				}
				_framePointer = (PUINT64)long1;
				continue;
			case 47://pget
				*_stack = **(PUINT64*)_stack;
				continue;
			case 48://pset
				_stack -= 2;
				**(PUINT64*)(_stack + 2) = *(_stack + 1);
				continue;
			case 49://ppeekset
				_stack--;
				**(PUINT64*)_stack = *(_stack + 1);
				continue;
			case 50://ToStack
				_stack -= 2;
				int1 = *(int*)(_stack + 1);//amount to push
				long1 = *(_stack + 2);//pointerloc
				while (int1-- > 0)
				{
					*(++_stack) = *(PUINT64)long1;
					long1 += 8;
				}
				continue;
			case 51://FromStack
				_stack -= 2;
				int1 = *(int*)(_stack + 1);//amount to pop
				long1 = *(_stack + 2);
				_stack -= int1;
				int2 = 1;
				while (int1-- > 0)
				{
					*(PUINT64)long1 = *(_stack + int2);
					long1 += 8;
					int2++;
				}
				continue;
			case 52://ArrayGetP1
				_stack--;
				long1 = *(_stack + 1);
				int1 = *(int*)_stack;
				*_stack = long1 + 8 + (UINT64)(int1 * (_base._codeTable[++_codeTableIndex] << 3));
				continue;
			case 73://ArrayGetP2
				_stack--;
				long1 = *(_stack + 1);
				int1 = *(int*)_stack;
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				*_stack = long1 + 8 + (UINT64)(int1 * (int2 << 3));
				continue;
			case 53://ArrayGet1
				_stack--;
				long1 = *(_stack + 1);
				int1 = *(int*)_stack;
				*_stack = *(PUINT64)(long1 + 8 + (UINT64)(int1 * (_base._codeTable[++_codeTableIndex] << 3)));
				continue;
			case 74://ArrayGet2
				_stack--;
				long1 = *(_stack + 1);
				int1 = *(int*)_stack;
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				*_stack = *(PUINT64)(long1 + 8 + (UINT64)(int1 * (int2 << 3)));
				continue;
			case 54://ArraySet1
				_stack -= 3;
				long1 = *(_stack + 3);
				int1 = *(int*)(_stack + 2);
				*(PUINT64)(long1 + 8 + (UINT64)(int1 * (_base._codeTable[++_codeTableIndex] << 3))) = *(_stack + 1);
				continue;
			case 75://ArraySet2
				_stack -= 3;
				long1 = *(_stack + 3);
				int1 = *(int*)(_stack + 2);
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				*(PUINT64)(long1 + 8 + (UINT64)(int1 * (int2 << 3))) = *(_stack + 1);
				continue;
			case 55://pframe1
				_stack++;
				*_stack = (UINT64)(_framePointer + _base._codeTable[++_codeTableIndex]);
				continue;
			case 76://pframe2
				_stack++;
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				*_stack = (UINT64)(_framePointer + int2);
				continue;
			case 56://getf1
				_stack++;
				*_stack = *(_framePointer + _base._codeTable[++_codeTableIndex]);
				continue;
			case 77://getf2
				_stack++;
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				*_stack = *(_framePointer + int2);
				continue;
			case 57://setf1
				_stack--;
				*(_framePointer + _base._codeTable[++_codeTableIndex]) = *(_stack + 1);
				continue;
			case 78://setf2
				_stack--;
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				*(_framePointer + int2) = *(_stack + 1);
				continue;
			case 58://pstatic1
				_stack++;
				long1 = (UINT64)_locals + (UINT64)(_base._codeTable[++_codeTableIndex] << 3);
				*_stack = long1;
				continue;
			case 79://pstatic2
				_stack++;
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				long1 = (UINT64)_locals + (UINT64)(int2 << 3);
				*_stack = long1;
				continue;
			case 59://staticget1
				_stack++;
				long1 = (UINT64)_locals + (UINT64)(_base._codeTable[++_codeTableIndex] << 3);
				*_stack = *(PUINT64)long1;
				continue;
			case 80://staticget2
				_stack++;
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				long1 = (UINT64)_locals + (UINT64)(int2 << 3);
				*_stack = *(PUINT64)long1;
				continue;
			case 60://staticset1
				_stack--;
				long1 = (UINT64)_locals + (UINT64)(_base._codeTable[++_codeTableIndex] << 3);
				*(PUINT64)long1 = *(_stack + 1);
				continue;
			case 81://staticset2
				_stack--;
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				long1 = (UINT64)_locals + (UINT64)(int2 << 3);
				*(PUINT64)long1 = *(_stack + 1);
				continue;
			case 61://Add1
				*(int*)_stack += _base._codeTable[++_codeTableIndex];
				continue;
			case 62://mul1 
				*(int*)_stack *= _base._codeTable[++_codeTableIndex];
				continue;
			case 63://GetImmP
				_stack--;
				int1 = *(int*)(_stack + 1);
				*_stack += (UINT64)(int1 << 3);
				continue;
			case 64://GetImmP1
				*_stack += (UINT64)(_base._codeTable[++_codeTableIndex] << 3);
				continue;
			case 70://GetImmP2
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				*_stack += (UINT64)(int2 << 3);
				continue;
			case 65://GetImm1
				*_stack = *(PUINT64)(*_stack + (UINT64)(_base._codeTable[++_codeTableIndex] << 3));
				continue;
			case 71://GetImm2
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				*_stack = *(PUINT64)(*_stack + (UINT64)(int2 << 3));
				continue;
			case 66://SetImm1
				_stack -= 2;
				*(PUINT64)(*(_stack + 2) + (UINT64)(_base._codeTable[++_codeTableIndex] << 3)) = *(_stack + 1);
				continue;
			case 72://SetImm2
				_stack -= 2;
				int2 = _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				*(PUINT64)(*(_stack + 2) + (UINT64)(int2 << 3)) = *(_stack + 1);
				continue;
			case 67://PushS
				_stack++;
				short1 = _base._codeTable[++_codeTableIndex] << 8;
				short1 |= _base._codeTable[++_codeTableIndex];
				*(int*)_stack = short1;
				continue;
			case 68://Add2
				short1 = _base._codeTable[++_codeTableIndex] << 8;
				short1 |= _base._codeTable[++_codeTableIndex];
				*(int*)_stack += short1;
				continue;
			case 69://Mul2
				short1 = _base._codeTable[++_codeTableIndex] << 8;
				short1 |= _base._codeTable[++_codeTableIndex];
				*(int*)_stack *= short1;
				continue;
			case 82://pglobal2
				_stack++;
				int1 = _base._codeTable[++_codeTableIndex] << 8;
				int1 |= _base._codeTable[++_codeTableIndex];
				*_stack = (UINT64)getGlobalPtr(int1);
				continue;
			case 94://pglobal3
				_stack++;
				int1 = _base._codeTable[++_codeTableIndex] << 16;
				int1 |= _base._codeTable[++_codeTableIndex] << 8;
				int1 |= _base._codeTable[++_codeTableIndex];
				*_stack = (UINT64)getGlobalPtr(int1);
				continue;
			case 83://globalget2
				_stack++;
				int1 = _base._codeTable[++_codeTableIndex] << 8;
				int1 |= _base._codeTable[++_codeTableIndex];
				*_stack = *getGlobalPtr(int1);
				continue;
			case 95://globalget3
				_stack++;
				int1 = _base._codeTable[++_codeTableIndex] << 16;
				int1 |= _base._codeTable[++_codeTableIndex] << 8;
				int1 |= _base._codeTable[++_codeTableIndex];
				*_stack = *getGlobalPtr(int1);
				continue;
			case 84://globalset2
				_stack--;
				int1 = _base._codeTable[++_codeTableIndex] << 8;
				int1 |= _base._codeTable[++_codeTableIndex];
				*getGlobalPtr(int1) = *(_stack + 1);
			case 96://globalset3
				_stack--;
				int1 = _base._codeTable[++_codeTableIndex] << 16;
				int1 |= _base._codeTable[++_codeTableIndex] << 8;
				int1 |= _base._codeTable[++_codeTableIndex];
				*getGlobalPtr(int1) = *(_stack + 1);
				continue;
			case 85://Jump
				short1 = _base._codeTable[++_codeTableIndex] << 8;
				short1 |= _base._codeTable[++_codeTableIndex];
				_codeTableIndex += short1;
				continue;
			case 86://JumpF
				_stack--;
				short1 = _base._codeTable[++_codeTableIndex] << 8;
				short1 |= _base._codeTable[++_codeTableIndex];
				if (*(int*)(_stack + 1) == 0)
					_codeTableIndex += short1;
				continue;
			case 87://JumpNE
				_stack -= 2;
				short1 = _base._codeTable[++_codeTableIndex] << 8;
				short1 |= _base._codeTable[++_codeTableIndex];
				if (*(int*)(_stack + 1) != *(int*)(_stack + 2))
					_codeTableIndex += short1;
				continue;
			case 88://JumpEQ
				_stack -= 2;
				short1 = _base._codeTable[++_codeTableIndex] << 8;
				short1 |= _base._codeTable[++_codeTableIndex];
				if (*(int*)(_stack + 1) == *(int*)(_stack + 2))
					_codeTableIndex += short1;
				continue;
			case 89://JumpLE
				_stack -= 2;
				short1 = _base._codeTable[++_codeTableIndex] << 8;
				short1 |= _base._codeTable[++_codeTableIndex];
				if (*(int*)(_stack + 1) <= *(int*)(_stack + 2))
					_codeTableIndex += short1;
				continue;
			case 90://JumpLT
				_stack -= 2;
				short1 = _base._codeTable[++_codeTableIndex] << 8;
				short1 |= _base._codeTable[++_codeTableIndex];
				if (*(int*)(_stack + 1) < *(int*)(_stack + 2))
					_codeTableIndex += short1;
				continue;
			case 91://JumpGE
				_stack -= 2;
				short1 = _base._codeTable[++_codeTableIndex] << 8;
				short1 |= _base._codeTable[++_codeTableIndex];
				if (*(int*)(_stack + 1) >= *(int*)(_stack + 2))
					_codeTableIndex += short1;
				continue;
			case 92://JumpGT
				_stack -= 2;
				short1 = _base._codeTable[++_codeTableIndex] << 8;
				short1 |= _base._codeTable[++_codeTableIndex];
				if (*(int*)(_stack + 1) > *(int*)(_stack + 2))
					_codeTableIndex += short1;
				continue;
			case 93://Call
				*(int*)(_stack + 1) = _codeTableIndex + 3;
				int1 = _base._codeTable[++_codeTableIndex] << 16;
				int1 |= _base._codeTable[++_codeTableIndex] << 8;
				int1 |= _base._codeTable[++_codeTableIndex];
				_codeTableIndex = int1 - 1;
				continue;
			case 97://PushI24
				_stack++;
				int2 = _base._codeTable[++_codeTableIndex] << 16;
				int2 |= _base._codeTable[++_codeTableIndex] << 8;
				int2 |= _base._codeTable[++_codeTableIndex];
				*(int*)_stack = int2;
				continue;
			case 98://Switch
				_stack--;
				int1 = *(int*)(_stack + 1);//compare value
				int2 = _base._codeTable[++_codeTableIndex];//count
				for (int i = 0; i < int2; i++)
				{
					int3 = _base._codeTable[++_codeTableIndex] << 24;
					int3 |= _base._codeTable[++_codeTableIndex] << 16;
					int3 |= _base._codeTable[++_codeTableIndex] << 8;
					int3 |= _base._codeTable[++_codeTableIndex]; //switch case
					if (int1 == int3)
					{
						short1 = _base._codeTable[++_codeTableIndex] << 8;
						short1 |= _base._codeTable[++_codeTableIndex];//jump off
						_codeTableIndex += short1;
						break;
					}
					_codeTableIndex += 2;
				}
				continue;
			case 99://PushString
				int1 = *(int*)_stack;
				*_stack = (UINT64)_base._strings + (UINT64)int1;
				continue;
			case 100://GetHashKey
				nativeInit(0xD24D37CC275948CC);
				nativePush64(*_stack);
				*_stack = *nativeCall();
				continue;
			case 101://StrCopy
				_stack -= 2;
				ptr1 = (unsigned char*)(*(_stack + 1));
				ptr2 = (unsigned char*)(*(_stack + 2));
				int1 = _base._codeTable[++_codeTableIndex];
				do
				{
					if (--int1 == 0)
						break;
					*ptr2 = *ptr1;
					ptr2++;
					ptr1++;
				} while (*ptr1 != 0);
				*ptr2 = 0;
				continue;
			case 102://ItoS
				_stack -= 2;
				int1 = *(int*)(_stack + 1);
				ptr1 = (unsigned char*)(*(_stack + 2));
				int2 = _base._codeTable[++_codeTableIndex];
				ptr2 = buffer1;
				IntToString(int1, ptr2);
				while (*ptr2 != 0)
				{
					if (--int2 <= 0)
						break;
					*ptr1++ = *ptr2++;
				}
				*ptr1 = 0;
				continue;
				case 103://StrAdd
					_stack -= 2;
					ptr1 = (unsigned char*)(*(_stack + 1));
					ptr2 = (unsigned char*)(*(_stack + 2));
				StrAdd:
					int1 = _base._codeTable[++_codeTableIndex];
					while (*ptr2 != 0)
					{
						ptr2++;
						int1--;
					}
					while (*ptr1 != 0)
					{
						if (--int1 <= 0)
							break;
						*ptr2++ = *ptr1++;
					}
					*ptr2 = 0;
					continue;
				case 104://StrAddI
					_stack -= 2;
					ptr1 = buffer1;
					IntToString(*(int*)(_stack + 1), ptr1);
					ptr2 = (unsigned char*)(*(_stack + 2));
					goto StrAdd;
			case 105://MemCpy
				_stack -= 3;
				long1 = *(_stack + 3);
				int1 = *(int*)(_stack + 2);
				int2 = *(int*)(_stack + 1);

				if (int2 > int1)
				{
					_stack += int2 - int1;
					int2 = int1;
				}
				if (int2 > 0)
				{
					int3 = int2;
					long2 = long1 + (UINT64)(8 * (int2 - 1));
					do
					{
						*(PUINT64)long2 = *_stack;
						_stack--;
						long2 -= 8;
						int3--;
					} while (int3 >= 0);
					*(unsigned char*)(long1 + (UINT64)(8 * (int2 - 1))) = 0;
				}
				continue;
			case 106://Catch
				//Made into breakpoint
				BreakPoint(true);
				continue;
			case 107://Trhow
				BreakPoint(false);
				continue;
			case 108://pCall
				_stack--;
				int1 = *(int*)(_stack + 1);
				*(int*)(_stack + 1) = _codeTableIndex;
				_codeTableIndex = int1 - 1;

				continue;
			case 109:
			case 110:
			case 111:
			case 112:
			case 113:
			case 114:
			case 115:
			case 116:
			case 117:
				_stack++;
				*(int*)_stack = _base._codeTable[_codeTableIndex] - 110;
				continue;
			case 118:
				_stack++;
				*(float*)_stack = -1.0f;
				continue;
			case 119:
				_stack++;
				*(float*)_stack = 0.0f;
				continue;
			case 120:
				_stack++;
				*(float*)_stack = 1.0f;
				continue;
			case 121:
				_stack++;
				*(float*)_stack = 2.0f;
				continue;
			case 122:
				_stack++;
				*(float*)_stack = 3.0f;
				continue;
			case 123:
				_stack++;
				*(float*)_stack = 4.0f;
				continue;
			case 124:
				_stack++;
				*(float*)_stack = 5.0f;
				continue;
			case 125:
				_stack++;
				*(float*)_stack = 6.0f;
				continue;
			case 126:
				_stack++;
				*(float*)_stack = 7.0f;
				continue;
			}
		}
		catch(...)
		{
			Log(_base._Name + ": Exception caught, current opcode pointer = " + HexToString(_codeTableIndex) + ", opcode = " + HexToString(_base._codeTable[_codeTableIndex]));
			Terminate();
			return true;
		}
	}
}

void runningScript::IntToString(int value, unsigned char* ptr)
{
	unsigned char* ptr2 = buffer2;
	int count = 0;
	if (value == 0)
	{
		*(ptr++) = 48;
		*ptr = 0;
		return;
	}
	if (value < 0)
	{
		value = -value;
		*(ptr++) = 45;
	}
	while (value != 0)
	{
		*ptr2++ = (value % 10 + 48);
		value /= 10;
		count++;
	}
	while (count > 0)
	{
		count--;
		*ptr++ = *--ptr2;
	}
	*ptr = 0;
	return;

}

script::~script()
{
	Unload();
}

runningScript::runningScript(script &base) : _base(base)
{
	if (!_base.isLoaded)
	{
		isRunning = false;
		Log(_base._Name + ": Script not loaded into memory, please use request_script first");
		return;
	}
	_locals = new UINT64[base.staticSize];
	memcpy(_locals, base._locals, 8 * base.staticSize);
	Log(_base._Name + ": Allocated locals at " + LongHexToString((UINT64)_locals));
	_codeTableIndex = 0;
	_stackPointer = new UINT64[1024];
	_stack = _stackPointer;
	*(int*)_stack = -1;
	_stack -= 1;
	_framePointer = _stack;
	waituntil = GetTickCount();
	isRunning = true;
	Log(_base._Name + ": Allocated stack at " + LongHexToString((UINT64)_stackPointer));
	buffer1 = new unsigned char[64];
	buffer2 = new unsigned char[64];
	debuglist = new unsigned char[100];
	_base.instances++;
}

runningScript::runningScript() : _base(script())
{
	isRunning = false;
}

runningScript::runningScript(runningScript & other) : isRunning(other.isRunning), ExitReason(other.ExitReason), _codeTableIndex(other._codeTableIndex), waituntil(other.waituntil), _base(other._base)
{
	_stackPointer = new UINT64[1024];
	memcpy(_stackPointer, other._stackPointer, 1024 * 8);
	_stack = _stackPointer + (other._stack - other._stackPointer);
	_framePointer = _stackPointer + (other._framePointer - other._stackPointer);
	_locals = new UINT64[other._base.staticSize];
	memcpy(_locals, other._locals, other._base.staticSize * 8);
	buffer1 = new unsigned char[64];
	buffer2 = new unsigned char[64];
	debuglist = new unsigned char[100];
	memcpy(debuglist, other.debuglist, 100);
}

runningScript::~runningScript()
{
	Terminate();
}


runningScripts::~runningScripts()
{
}

void runningScripts::Run()
{
	for (auto scr = scripts.begin(); scr != scripts.end(); ++scr)
	{
		Log("test");
		scr->Run();
	}
}

void runningScripts::StartNewScript(script& Script)
{
	if (!Script.isLoaded)
	{
		Log(Script._Name + ": Cannot start script as it hasnt been loaded");
		return;
	}
	Log(Script._Name + ": Script Starting");
	scripts.push_back(runningScript(Script));
	Log(Script._Name + ": Added to active script collection");
}
