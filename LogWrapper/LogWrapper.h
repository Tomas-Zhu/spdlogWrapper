#ifdef LOGWRAPPER_EXPORTS
#define LOGWRAPPER_API __declspec(dllexport)
#elif defined LOGWRAPPER_LIB
#define LOGWRAPPER_API
#else
#define LOGWRAPPER_API __declspec(dllimport)
#endif

#ifndef SPDLOG_WCHAR_TO_UTF8_SUPPORT
#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#endif

#ifndef SPDLOG_WCHAR_FILENAMES
#define SPDLOG_WCHAR_FILENAMES
#endif

#ifndef _SCL_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <memory>
#include <vector>
#include <utility>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/stopwatch.h>

typedef std::pair<std::string, std::wstring> LogPathItem;

namespace LogWrapper
{
	enum LogType
	{
		Log_Debug = 1,
		Log_Desc,
		Log_Warning,
		Log_Error,
		Log_Critical,
	};

	LOGWRAPPER_API void Init(const std::vector<LogPathItem>& logPathItems);
	LOGWRAPPER_API void Uninit();
	LOGWRAPPER_API void SetDefaultLogger(const std::string& logName);
	LOGWRAPPER_API std::string GetDefaultLoggerName();
	LOGWRAPPER_API void SetLogLevel(const std::string& logName, LogType type);
	LOGWRAPPER_API void FlushLog(const std::string& logName);
	LOGWRAPPER_API void WriteLogA(const std::string& logName, LogType type, const char* log, ...);
	LOGWRAPPER_API void WriteLogA(const std::string& logName, LogType type, const char* log, va_list args);
	LOGWRAPPER_API void WriteLogW(const std::string& logName, LogType type, const wchar_t* log, ...);
	LOGWRAPPER_API void WriteLogW(const std::string& logName, LogType type, const wchar_t* log, va_list args);
	LOGWRAPPER_API std::wstring GetLogPath(const std::string& logName);

	class CStopWatcherImpl;
	class LOGWRAPPER_API CStopWatcher
	{
	public:
		CStopWatcher(const std::string& log);
		CStopWatcher(const std::wstring& log);
		CStopWatcher(const std::string& logName, const std::string& log);
		CStopWatcher(const std::string& logName, const std::wstring& log);
		virtual ~CStopWatcher();
	private:
		CStopWatcherImpl* m_impl;
	};
};

#define DEBUG_A(fm,...) LogWrapper::WriteLogA(LogWrapper::GetDefaultLoggerName(), LogWrapper::Log_Debug, fm, __VA_ARGS__);
#define DEBUG_W(fm,...) LogWrapper::WriteLogW(LogWrapper::GetDefaultLoggerName(), LogWrapper::Log_Debug, fm, __VA_ARGS__); 
#define DESC_A(fm,...) LogWrapper::WriteLogA(LogWrapper::GetDefaultLoggerName(), LogWrapper::Log_Desc, fm, __VA_ARGS__);
#define DESC_W(fm,...) LogWrapper::WriteLogW(LogWrapper::GetDefaultLoggerName(), LogWrapper::Log_Desc, fm, __VA_ARGS__);
#define WARN_A(fm,...) LogWrapper::WriteLogA(LogWrapper::GetDefaultLoggerName(), LogWrapper::Log_Warning, fm, __VA_ARGS__);
#define WARN_W(fm,...) LogWrapper::WriteLogW(LogWrapper::GetDefaultLoggerName(), LogWrapper::Log_Warning, fm, __VA_ARGS__);
#define ERROR_A(fm,...) LogWrapper::WriteLogA(LogWrapper::GetDefaultLoggerName(), LogWrapper::Log_Error, fm, __VA_ARGS__);
#define ERROR_W(fm,...) LogWrapper::WriteLogW(LogWrapper::GetDefaultLoggerName(), LogWrapper::Log_Error, fm, __VA_ARGS__);
#define CRITICAL_A(fm,...) LogWrapper::WriteLogA(LogWrapper::GetDefaultLoggerName(), LogWrapper::Log_Critical, fm, __VA_ARGS__);
#define CRITICAL_W(fm,...) LogWrapper::WriteLogW(LogWrapper::GetDefaultLoggerName(), LogWrapper::Log_Critical, fm, __VA_ARGS__);