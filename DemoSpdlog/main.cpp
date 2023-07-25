#define _SCL_SECURE_NO_WARNINGS
#include "LogWrapper.h"

int main()
{
	LogPathItem item = { "DemoSpdlog",L"D:/spdlog.txt" };
	LogWrapper::Init({ item });
	while (true)
	{
#ifdef STOP_WATCH_TEST
		{
			LogWrapper::CStopWatcher watcher = LogWrapper::CStopWatcher("", "stopwatcher test");
			Sleep(1000);
		}
		Sleep(20);
#else
		LogWrapper::WriteLogA("DemoSpdlog", LogWrapper::Log_Critical, u8"benchmark test");
#endif // STOP_WATCH_TEST
	}
	LogWrapper::Uninit();
	return 0;
}