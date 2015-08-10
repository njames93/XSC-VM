#include "xscScript.h"
#include "main.h"
#include "NativeTranslation.h"
#include "script.h"
#include <sstream>

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

xscHeader::xscHeader(std::ifstream& file) : _file(file), buffer(new unsigned char[4])
{
	rsc7Off = readuint(0) == 0x52534337 ? 0x10 : 0x0;
	codeOff = readuintptr(0x8 + rsc7Off) + rsc7Off;
	codeSize = readuint(0x10 + rsc7Off);
	staticSize = readuint(0x18 + rsc7Off);
	nativeSize = readuint(0x20 + rsc7Off);
	staticOff = readuintptr(0x24 + rsc7Off) + rsc7Off;
	nativeOff = readuintptr(0x2c + rsc7Off) + rsc7Off;
	stringsOff = readuintptr(0x44 + rsc7Off) + rsc7Off;
	stringSize = readuint(0x48 + rsc7Off);
}

UINT xscHeader::readuint(int position)
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
UINT xscHeader::readuintptr(int position)
{
	_file.clear();
	_file.seekg(position, std::ios_base::beg);
	_file.read((char *)buffer, 4);
	UINT ret = buffer[1] << 16;
	ret |= buffer[2] << 8;
	ret |= buffer[3];
	return ret;
}
bool xscHeader::fillNative(PUINT64 Target)
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
			Log("Error with native " + HexToString(hash) + ", Native read from " + HexToString(nativeOff) + " + " + HexToString(4 * i));
			return false;
		}
	}
	return true;
}

void xscHeader::fillCode(unsigned char* Target)
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

void xscHeader::fillStrings(unsigned char* Target)
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

void xscHeader::fillStatics(PUINT64 Target)
{
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

xscScript::xscScript(std::string file, std::string Name) : _Name(Name)
{
	Log(_Name + ": Initialising");
	std::ifstream xscFile;
	xscFile.open(file, std::ios::in | std::ios::binary | std::ios::ate);
	xscHeader header = xscHeader(xscFile);
	Log(_Name + ": CodeSize = " + std::to_string(header.codeSize) + ", StringSize = " + std::to_string(header.stringSize) + ", LocalSize = " + std::to_string(header.staticSize)+", NativeSize = " + std::to_string(header.nativeSize));
	//read xscfile into memory
	maxcodesize = header.codeSize;
	_locals = new UINT64[header.staticSize];
	_strings = new unsigned char[header.stringSize];
	_codeTable = new unsigned char[header.codeSize];
	NativeHashes = new UINT64[header.nativeSize];
	_codeTableIndex = 0;
	header.fillStatics(_locals);
	if (header.staticSize > 0)
		Log(_Name + ": Allocated statics at " + LongHexToString((UINT64)_locals));
	header.fillStrings(_strings);
	if (header.stringSize > 0)
		Log(_Name + ": Allocated strings at " + LongHexToString((UINT64)_strings));
	header.fillCode(_codeTable);
	Log(_Name + ": Allocated code segment at " + LongHexToString((UINT64)_codeTable));
	if (header.fillNative(NativeHashes))
	{
		waituntil = GetTickCount();
		isRunning = true;
		Log(_Name + ": Allocated native table at " + LongHexToString((UINT64)NativeHashes));
		//generate stack and string buffers
		_stackPointer = new UINT64[1024];
		Log(_Name + ": Allocated stack at " + LongHexToString((UINT64)_stackPointer));
		buffer1 = new unsigned char[64];
		buffer2 = new unsigned char[64];

		_stack = _stackPointer;
		*(int*)_stack = -1;
		_stack -= 1;
		_framePointer = _stack;
	}
	else
	{
		ExitReason = "Unknown native prevented script from starting";
		isRunning = false;
		delete _locals;
		Log(_Name + ": Freeing up Locals");
		delete _strings;
		Log(_Name + ": Freeing up strings");
		delete _codeTable;
		Log(_Name + ": Freeing up code table");
		delete NativeHashes;
		Log(_Name + ": Freeing up native table");
	}
	xscFile.close();

}

void xscScript::Terminate()
{
	delete _stackPointer;
	delete _locals;
	delete _strings;
	delete buffer1;
	delete buffer2;
	delete _codeTable;
	delete NativeHashes;
	isRunning = false;
}

bool xscScript::OnTick()
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
		//Log(HexToString(_codeTableIndex) + " opcode = " + HexToString(_codeTable[_codeTableIndex]));
		switch (_codeTable[_codeTableIndex])
		{
		case 0:
			if (_codeTableIndex > maxcodesize || _codeTableIndex < 0)//sanity check
			{
				ExitReason = "Exceeded code length, aborting";
				Terminate();
				return false;
			}//no-op - checked
			continue;
		case 1://iadd - checked
			_stack--;
			*(int*)_stack += *(int*)(_stack + 1);
			continue;
		case 2://isub - checked
			_stack--;
			*(int*)_stack -= *(int*)(_stack + 1);
			continue;
		case 3://imul - checked
			_stack--;
			*(int*)_stack *= *(int*)(_stack + 1);
			continue;
		case 4://idiv - checked
			_stack--;
			if (*(int*)(_stack + 1) != 0)
				*(int*)_stack /= *(int*)(_stack + 1);
			continue;
		case 5://imod - checked
			_stack--;
			if (*(int*)(_stack + 1) != 0)
				*(int*)_stack %= *(int*)(_stack + 1);
			continue;
		case 6://inot - checked
			*(int*)_stack = (*(int*)_stack == 0) ? 1 : 0;
			continue;
		case 7://ineg - checked
			*(int*)_stack = -*(int*)_stack;
			continue;
		case 8://icmpeq - checked
			_stack--;
			*(int*)_stack = (*(int*)_stack == *(int*)(_stack + 1)) ? 1 : 0;
			continue;
		case 9://icmpne - checked
			_stack--;
			*(int*)_stack = (*(int*)_stack != *(int*)(_stack + 1)) ? 1 : 0;
			continue;
		case 10://icmpgt - checked
			_stack--;
			*(int*)_stack = (*(int*)_stack > *(int*)(_stack + 1)) ? 1 : 0;
			continue;
		case 11://icmpge - checked
			_stack--;
			*(int*)_stack = (*(int*)_stack >= *(int*)(_stack + 1)) ? 1 : 0;
			continue;
		case 12://icmplt - checked
			_stack--;
			*(int*)_stack = (*(int*)_stack < *(int*)(_stack + 1)) ? 1 : 0;
			continue;
		case 13://icmple - checked
			_stack--;
			*(int*)_stack = (*(int*)_stack <= *(int*)(_stack + 1)) ? 1 : 0;
			continue;
		case 14://fadd - checked
			_stack--;
			*(float*)_stack += *(float*)(_stack + 1);
			continue;
		case 15://fsub - checked
			_stack--;
			*(float*)_stack -= *(float*)(_stack + 1);
			continue;
		case 16://fmul - checked
			_stack--;
			*(float*)_stack *= *(float*)(_stack + 1);
			continue;
		case 17://fdiv - checked
			_stack--;
			if (*(int*)(_stack + 1) != 0)
				*(float*)_stack /= *(float*)(_stack + 1);
			continue;
		case 18://fmod - checked
			_stack--;
			if (*(int*)(_stack + 1) != 0)
			{
				float1 = *(float *)_stack;
				float2 = *(float*)(_stack + 1);
				*(float*)_stack -= (float)(((int)(float1 / float2)) * float2);
			}
			continue;
		case 19://fneg - checked
			*(int*)_stack ^= 1 << 31;
			continue;
		case 20://fcmpeq - checked
			_stack--;
			*(int*)_stack = (*(float*)_stack == *(float*)(_stack + 1)) ? 1 : 0;
			continue;
		case 21://fcmpne - checked
			_stack--;
			*(int*)_stack = (*(float*)_stack != *(float*)(_stack + 1)) ? 1 : 0;
			continue;
		case 22://fcmpgt - checked
			_stack--;
			*(int*)_stack = (*(float*)_stack > *(float*)(_stack + 1)) ? 1 : 0;
			continue;
		case 23://fcmpge - checked
			_stack--;
			*(int*)_stack = (*(float*)_stack >= *(float*)(_stack + 1)) ? 1 : 0;
			continue;
		case 24://fcmplt - checked
			_stack--;
			*(int*)_stack = (*(float*)_stack < *(float*)(_stack + 1)) ? 1 : 0;
			continue;
		case 25://fcmple - checked
			_stack--;
			*(int*)_stack = (*(float*)_stack <= *(float*)(_stack + 1)) ? 1 : 0;
			continue;
		case 26://vadd - checked
			_stack -= 3;
			*(float*)_stack += *(float*)(_stack + 3);
			*(float*)(_stack - 1) += *(float*)(_stack + 2);
			*(float*)(_stack - 2) += *(float*)(_stack + 1);
			continue;
		case 27://vsub - checked
			_stack -= 3;
			*(float*)_stack -= *(float*)(_stack + 3);
			*(float*)(_stack - 1) -= *(float*)(_stack + 2);
			*(float*)(_stack - 2) -= *(float*)(_stack + 1);
			continue;
		case 28://vmul - checked
			_stack -= 3;
			*(float*)_stack *= *(float*)(_stack + 3);
			*(float*)(_stack - 1) *= *(float*)(_stack + 2);
			*(float*)(_stack - 2) *= *(float*)(_stack + 1);
			continue;
		case 29://vdiv - checked
			_stack -= 3;
			if (*(int*)(_stack + 3) != 0)
				*(float*)_stack /= *(float*)(_stack + 3);
			if (*(int*)(_stack + 2) != 0)
				*(float*)(_stack - 1) /= *(float*)(_stack + 2);
			if (*(int*)(_stack + 1) != 0)
				*(float*)(_stack - 2) /= *(float*)(_stack + 1);
			continue;
		case 30://vneg - checked
			*(int*)_stack ^= 1 << 31;
			*(int*)(_stack - 1) ^= 1 << 31;
			*(int*)(_stack - 2) ^= 1 << 31;
			continue;
		case 31://and - checked
			_stack--;
			*(int*)_stack &= *(int*)(_stack + 1);
			continue;
		case 32://or - checked
			_stack--;
			*(int*)_stack |= *(int*)(_stack + 1);
			continue;
		case 33://xor - checked
			_stack--;
			*(int*)_stack ^= *(int*)(_stack + 1);
			continue;
		case 34://ItoF - checked
			*(float*)_stack = (float)(*(int*)_stack);
			continue;
		case 35://FtoI
			*(int*)_stack = (int)(*(float*)_stack);
			continue;
		case 36://FtoV - checked
			_stack += 2;
			*(int*)_stack = *(int*)(_stack - 2);
			*(int*)(_stack - 1) = *(int*)(_stack - 2);
			continue;
		case 37://Push1 - checked
			_stack++;
			*(int*)_stack = _codeTable[++_codeTableIndex];
			continue;
		case 38://Push2 - checked
			_stack += 2;
			*(int*)(_stack - 1) = _codeTable[++_codeTableIndex];
			*(int*)_stack = _codeTable[++_codeTableIndex];
			continue;
		case 39://Push3 - checked
			_stack += 3;
			*(int*)(_stack - 2) = _codeTable[++_codeTableIndex];
			*(int*)(_stack - 1) = _codeTable[++_codeTableIndex];
			*(int*)_stack = _codeTable[++_codeTableIndex];
			continue;
		case 40://ipush - checked
		case 41://fpush - checked
			_stack++;
			int1 = _codeTable[++_codeTableIndex] << 24;
			int1 |= _codeTable[++_codeTableIndex] << 16;
			int1 |= _codeTable[++_codeTableIndex] << 8;
			int1 |= _codeTable[++_codeTableIndex];
			*(int*)_stack = int1;
			continue;
		case 42://dup - checked
			_stack++;
			*(int*)_stack = *(int*)(_stack - 1);
			continue;
		case 43://drop - checked
			_stack--;
			continue;
		case 44://callnative
			int1 = _codeTable[++_codeTableIndex];//param/return byte
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];//native table index
			long1 = NativeHashes[int2];//get native hash
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
				*_stack = (UINT64)_Name.c_str();
				continue;
			case 0x1090044AD1DA76FA://terminate this thread
				Terminate();
				return true;
			case 0x8A1C8B1738FFE87E://get script name hash
				_stack++;
				nativeInit(0xD24D37CC275948CC);
				nativePush64((UINT64)_Name.c_str());
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
			//Log(_Name + ": NativeCall: Params = " + std::to_string(int3) + ", returns = " + std::to_string(int4) + "\nChange in stack = " + LongHexToString(long3 - (UINT64)_stack) + "\ncurrent stack pointer = " + LongHexToString((UINT64)_stack));//debug message, checked and results what was expected
			continue;
		case 45://Function - checked
			int1 = _codeTable[++_codeTableIndex];//paramcount
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];//varcount
			int3 = _codeTable[++_codeTableIndex];
			_codeTableIndex += int3;
			*(_stack + 2) = (UINT64)_framePointer;
			_framePointer = _stack - int1 + 1;
			_stack += int2;
			continue;
		case 46://Ret - checked
			int1 = _codeTable[++_codeTableIndex];//paramcount
			int2 = _codeTable[++_codeTableIndex];//retcount
			_codeTableIndex = *(int*)(_framePointer + int1);
			if (_codeTableIndex == -1)
			{
				Terminate();
				ExitReason = "Return from main";
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
				"Return from main";
				return false;
			}
			_framePointer = (PUINT64)long1;
			continue;
		case 47://pget - checked
			*_stack = **(PUINT64*)_stack;
			continue;
		case 48://pset - checked
			_stack -= 2;
			**(PUINT64*)(_stack + 2) = *(_stack + 1);
			continue;
		case 49://ppeekset - checked
			_stack--;
			**(PUINT64*)_stack = *(_stack + 1);
			continue;
		case 50://ToStack - checked
			_stack -= 2;
			int1 = *(int*)(_stack + 1);//amount to push
			long1 = *(_stack + 2);//pointerloc
			while (int1-- > 0)
			{
				*(++_stack) = *(PUINT64)long1;
				long1 += 8;
			}
			continue;
		case 51://FromStack - checked
			_stack -= 2;
			int1 = *(int*)_stack + 1;//amount to pop
			long1 = *(_stack + 2);
			long2 = long1 + (UINT64)(int1 << 3);//pointerloc
			while (int1-- > 0)
			{
				long2 -= 8;
				*(PUINT64)long2 = *(_stack--);
			}
			continue;
		case 52://ArrayGetP1 - checked
			_stack--;
			long1 = *(_stack + 1);
			int1 = *(int*)_stack;
			*_stack = long1 + 8 + (UINT64)(int1 * (_codeTable[++_codeTableIndex] << 3));
			continue;
		case 73://ArrayGetP2 - checked
			_stack--;
			long1 = *(_stack + 1);
			int1 = *(int*)_stack;
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			*_stack = long1 + 8 + (UINT64)(int1 * (int2 << 3));
			continue;
		case 53://ArrayGet1 - checked
			_stack--;
			long1 = *(_stack + 1);
			int1 = *(int*)_stack;
			*_stack = *(PUINT64)(long1 + 8 + (UINT64)(int1 * (_codeTable[++_codeTableIndex] << 3)));
			continue;
		case 74://ArrayGet2 - checked
			_stack--;
			long1 = *(_stack + 1);
			int1 = *(int*)_stack;
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			*_stack = *(PUINT64)(long1 + 8 + (UINT64)(int1 * (int2 << 3)));
			continue;
		case 54://ArraySet1 - checked
			_stack -= 3;
			long1 = *(_stack + 3);
			int1 = *(int*)(_stack + 2);
			*(PUINT64)(long1 + 8 + (UINT64)(int1 * (_codeTable[++_codeTableIndex] << 3))) = *(_stack + 1);
			continue;
		case 75://ArraySet2 - checked
			_stack -= 3;
			long1 = *(_stack + 3);
			int1 = *(int*)(_stack + 2);
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			*(PUINT64)(long1 + 8 + (UINT64)(int1 * (int2 << 3))) = *(_stack + 1);
			continue;
		case 55://pframe1 - checked
			_stack++;
			*_stack = (UINT64)(_framePointer + _codeTable[++_codeTableIndex]);
			continue;
		case 76://pframe2 - checked
			_stack++;
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			*_stack = (UINT64)(_framePointer + int2);
			continue;
		case 56://getf1 - checked
			_stack++;
			*_stack = *(_framePointer + _codeTable[++_codeTableIndex]);
			continue;
		case 77://getf2 - checked
			_stack++;
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			*_stack = *(_framePointer + int2);
			continue;
		case 57://setf1 - checked
			_stack--;
			*(_framePointer + _codeTable[++_codeTableIndex]) = *(_stack + 1);
			continue;
		case 78://setf2 - checked
			_stack--;
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			*(_framePointer + int2) = *(_stack + 1);
			continue;
		case 58://pstatic1 - checked
			_stack++;
			long1 = (UINT64)_locals + (UINT64)(_codeTable[++_codeTableIndex] << 3);
			*_stack = long1;
			continue;
		case 79://pstatic2 - checked
			_stack++;
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			long1 = (UINT64)_locals + (UINT64)(int2 << 3);
			*_stack = long1;
			continue;
		case 59://staticget1 - checked
			_stack++;
			long1 = (UINT64)_locals + (UINT64)(_codeTable[++_codeTableIndex] << 3);
			*_stack = *(PUINT64)long1;
			continue;
		case 80://staticget2 - checked
			_stack++;
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			long1 = (UINT64)_locals + (UINT64)(int2 << 3);
			*_stack = *(PUINT64)long1;
			continue;
		case 60://staticset1 - checked
			_stack--;
			long1 = (UINT64)_locals + (UINT64)(_codeTable[++_codeTableIndex] << 3);
			*(PUINT64)long1 = *(_stack + 1);
			continue;
		case 81://staticset2 - checked
			_stack--;
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			long1 = (UINT64)_locals + (UINT64)(int2 << 3);
			*(PUINT64)long1 = *(_stack + 1);
			continue;
		case 61://Add1 - checked
			*(int*)_stack += _codeTable[++_codeTableIndex];
			continue;
		case 62://mul1  - checked
			*(int*)_stack *= _codeTable[++_codeTableIndex];
			continue;
		case 63://GetImmP - checked
			_stack--;
			int1 = *(int*)(_stack + 1);
			*_stack += (UINT64)(int1 << 3);
			continue;
		case 64://GetImmP1 - checked
			*_stack += (UINT64)(_codeTable[++_codeTableIndex] << 3);
			continue;
		case 70://GetImmP2 - checked
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			*_stack += (UINT64)(int2 << 3);
			continue;
		case 65://GetImm1 - checked
			*_stack = *(PUINT64)(*_stack + (UINT64)(_codeTable[++_codeTableIndex] << 3));
			continue;
		case 71://GetImm2 - checked
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			*_stack = *(PUINT64)(*_stack + (UINT64)(int2 << 3));
			continue;
		case 66://SetImm1 - checked
			_stack -= 2;
			*(PUINT64)(*(_stack + 2) + (UINT64)(_codeTable[++_codeTableIndex] << 3)) = *(_stack + 1);
			continue;
		case 72://SetImm2 - checked
			_stack -= 2;
			int2 = _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			*(PUINT64)(*(_stack + 2) + (UINT64)(int2 << 3)) = *(_stack + 1);
			continue;
		case 67://PushS - checked
			_stack++;
			short1 = _codeTable[++_codeTableIndex] << 8;
			short1 |= _codeTable[++_codeTableIndex];
			*(int*)_stack = short1;
			continue;
		case 68://Add2 - checked
			short1 = _codeTable[++_codeTableIndex] << 8;
			short1 |= _codeTable[++_codeTableIndex];
			*(int*)_stack += short1;
			continue;
		case 69://Mul2 - checked
			short1 = _codeTable[++_codeTableIndex] << 8;
			short1 |= _codeTable[++_codeTableIndex];
			*(int*)_stack *= short1;
			continue;
		case 82://pglobal2
			_stack++;
			int1 = _codeTable[++_codeTableIndex] << 8;
			int1 |= _codeTable[++_codeTableIndex];
			*_stack = (UINT64)getGlobalPtr(int1);
			continue;
		case 94://pglobal3
			_stack++;
			int1 = _codeTable[++_codeTableIndex] << 16;
			int1 |= _codeTable[++_codeTableIndex] << 8;
			int1 |= _codeTable[++_codeTableIndex];
			*_stack = (UINT64)getGlobalPtr(int1);
			continue;
		case 83://globalget2
			_stack++;
			int1 = _codeTable[++_codeTableIndex] << 8;
			int1 |= _codeTable[++_codeTableIndex];
			*_stack = *getGlobalPtr(int1);
			continue;
		case 95://globalget3
			_stack++;
			int1 = _codeTable[++_codeTableIndex] << 16;
			int1 |= _codeTable[++_codeTableIndex] << 8;
			int1 |= _codeTable[++_codeTableIndex];
			*_stack = *getGlobalPtr(int1);
			continue;
		case 84://globalset2
			_stack--;
			int1 = _codeTable[++_codeTableIndex] << 8;
			int1 |= _codeTable[++_codeTableIndex];
			*getGlobalPtr(int1) = *(_stack + 1);
		case 96://globalset3
			_stack--;
			int1 = _codeTable[++_codeTableIndex] << 16;
			int1 |= _codeTable[++_codeTableIndex] << 8;
			int1 |= _codeTable[++_codeTableIndex];
			*getGlobalPtr(int1) = *(_stack + 1);
			continue;
		case 85://Jump - checked
			short1 = _codeTable[++_codeTableIndex] << 8;
			short1 |= _codeTable[++_codeTableIndex];
			_codeTableIndex += short1;
			continue;
		case 86://JumpF - checked
			_stack--;
			short1 = _codeTable[++_codeTableIndex] << 8;
			short1 |= _codeTable[++_codeTableIndex];
			if (*(int*)(_stack + 1) == 0)
				_codeTableIndex += short1;
			continue;
		case 87://JumpNE - checked
			_stack -= 2;
			short1 = _codeTable[++_codeTableIndex] << 8;
			short1 |= _codeTable[++_codeTableIndex];
			if (*(int*)(_stack + 1) != *(int*)(_stack + 2))
				_codeTableIndex += short1;
			continue;
		case 88://JumpEQ - checked
			_stack -= 2;
			short1 = _codeTable[++_codeTableIndex] << 8;
			short1 |= _codeTable[++_codeTableIndex];
			if (*(int*)(_stack + 1) == *(int*)(_stack + 2))
				_codeTableIndex += short1;
			continue;
		case 89://JumpLE - checked
			_stack -= 2;
			short1 = _codeTable[++_codeTableIndex] << 8;
			short1 |= _codeTable[++_codeTableIndex];
			if (*(int*)(_stack + 1) <= *(int*)(_stack + 2))
				_codeTableIndex += short1;
			continue;
		case 90://JumpLT - checked
			_stack -= 2;
			short1 = _codeTable[++_codeTableIndex] << 8;
			short1 |= _codeTable[++_codeTableIndex];
			if (*(int*)(_stack + 1) < *(int*)(_stack + 2))
				_codeTableIndex += short1;
			continue;
		case 91://JumpGE - checked
			_stack -= 2;
			short1 = _codeTable[++_codeTableIndex] << 8;
			short1 |= _codeTable[++_codeTableIndex];
			if (*(int*)(_stack + 1) >= *(int*)(_stack + 2))
				_codeTableIndex += short1;
			continue;
		case 92://JumpGT - checked
			_stack -= 2;
			short1 = _codeTable[++_codeTableIndex] << 8;
			short1 |= _codeTable[++_codeTableIndex];
			if (*(int*)(_stack + 1) > *(int*)(_stack + 2))
				_codeTableIndex += short1;
			continue;
		case 93://Call - checked
			*(int*)(_stack + 1) = _codeTableIndex + 3;
			int1 = _codeTable[++_codeTableIndex] << 16;
			int1 |= _codeTable[++_codeTableIndex] << 8;
			int1 |= _codeTable[++_codeTableIndex];
			_codeTableIndex = int1 - 1;
			continue;
		case 97://PushI24 - checked
			_stack++;
			int2 = _codeTable[++_codeTableIndex] << 16;
			int2 |= _codeTable[++_codeTableIndex] << 8;
			int2 |= _codeTable[++_codeTableIndex];
			*(int*)_stack = int2;
			continue;
		case 98://Switch - checked
			_stack--;
			int1 = *(int*)(_stack + 1);//compare value
			int2 = _codeTable[++_codeTableIndex];//count
			for (int i = 0; i < int2; i++)
			{
				int3 = _codeTable[++_codeTableIndex] << 24;
				int3 |= _codeTable[++_codeTableIndex] << 16;
				int3 |= _codeTable[++_codeTableIndex] << 8;
				int3 |= _codeTable[++_codeTableIndex]; //switch case
				if (int1 == int3)
				{
					short1 = _codeTable[++_codeTableIndex] << 8;
					short1 |= _codeTable[++_codeTableIndex];//jump off
					_codeTableIndex += short1;
					break;
				}
				_codeTableIndex += 2;
			}
			continue;
		case 99://PushString
			int1 = *(int*)_stack;
			*_stack = (UINT64)_strings + (UINT64)int1;
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
			int1 = _codeTable[++_codeTableIndex];
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
			int2 = _codeTable[++_codeTableIndex];
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
		case 106://Throw
		case 107://catch
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
			*(int*)_stack = _codeTable[_codeTableIndex] - 110;
			continue;
		case 118:
			_stack++;
			*(UINT*)_stack = 0xBF800000;
			continue;
		case 119:
			_stack++;
			*(UINT*)_stack = 0;
			continue;
		case 120:
			_stack++;
			*(UINT*)_stack = 0xBF800000;
			continue;
		case 121:
			_stack++;
			*(UINT*)_stack = 0x40000000;
			continue;
		case 122:
			_stack++;
			*(UINT*)_stack = 0x40400000;
			continue;
		case 123:
			_stack++;
			*(UINT*)_stack = 0x40800000;
			continue;
		case 124:
			_stack++;
			*(UINT*)_stack = 0x40A00000;
			continue;
		case 125:
			_stack++;
			*(UINT*)_stack = 0x40C00000;
			continue;
		case 126:
			_stack++;
			*(UINT*)_stack = 0x40E00000;
			continue;
		}
	}
}

void xscScript::IntToString(int value, unsigned char* ptr)
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

xscScript::~xscScript()
{
}