#pragma once
#include <string>

void ScriptMain();
void Log(std::string Message);
#if !NDEBUG
void fastlog(char* pos);
#endif
