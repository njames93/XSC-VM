#pragma once
#include <map>

class NativeTranslation
{
public:
	static std::map<unsigned int, unsigned long long> Table;
	static bool Exists(unsigned int hash);
	static unsigned long long GetNative(unsigned int hash);
};

