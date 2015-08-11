#include "script.h"
#include "keyboard.h"

#include <string>
#include "xscScript.h"
#include "main.h"
#include "natives.h"
#include <ostream>
#include <stdio.h>

#pragma warning(disable : 4244 4305) // double <-> float conversions
std::ofstream _fastLog;
void Log(std::string Message)
{
	std::ofstream Log;
	Log.open("XSCVM.log", std::ios_base::app);
	Log << Message + "\n";
	Log.close();
}
#if !NDEBUG
void fastlog(char *val)
{
	_fastLog.write(val, 4);
}
#endif

void ResetLog()
{
	remove("XSCVM.log");
#if !NDEBUG
	remove("XSCVM_fast.log");
	_fastLog.open("XSCVM_fast.log", std::ios::out | std::ios::binary | std::ios::ate);
#endif
}

void main()
{
	ResetLog();
	xscScript Script = xscScript("main.xsc", "Main");
	if (!Script.isRunning)
	{
		if (Script.ExitReason.length() > 1)
			Log("Main: " + Script.ExitReason);
		else
			Log("Main: Unknown error prevented script from starting");
	}
	else
	{
		Log("Main: Script started");
	}
	while (true)
	{
		if (Script.isRunning)
		{
			Script.OnTick();
			if (!Script.isRunning)
				Log(Script.ExitReason);
		}
		else
			GRAPHICS::DRAW_RECT(0.5, 0.5, 0.5, 0.5, 0, 0, 255, 255);
		WAIT(0);
	}
}


void ScriptMain()
{
	srand(GetTickCount());
	main();
}
