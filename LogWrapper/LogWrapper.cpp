#include "stdafx.h"
#include "LogWrapper.h"
#include "compressed_rotating_file_sink.hpp"
#include <spdlog/async.h>
#include <fmt/chrono.h>

inline std::shared_ptr<spdlog::logger> GetLogger(const std::string& logName)
{
	std::weak_ptr<spdlog::logger> logPtr = spdlog::get(logName);
	if (logPtr.expired())
	{
		return nullptr;
	}
	return logPtr.lock();
}

inline spdlog::level::level_enum GetSpdLogLevel(LogWrapper::LogType type)
{
	spdlog::level::level_enum lv = spdlog::level::level_enum::n_levels;
	switch (type)
	{
	case LogWrapper::Log_Debug:
		lv = spdlog::level::debug;
		break;
	case LogWrapper::Log_Desc:
		lv = spdlog::level::info;
		break;
	case LogWrapper::Log_Warning:
		lv = spdlog::level::warn;
		break;
	case LogWrapper::Log_Error:
		lv = spdlog::level::err;
		break;
	case LogWrapper::Log_Critical:
		lv = spdlog::level::critical;
		break;
	default:
		break;
	}
	return lv;
}

LOGWRAPPER_API void LogWrapper::Init(const std::vector<LogPathItem>& logPathItems)
{
	// 200MB
	const int rotated_max_size = 1024 * 1024 * 200;
	const int rotated_max_files = 1;
	const int compressed_max_files = 1;

	for (auto it : logPathItems)
	{
		if (spdlog::get(it.first))
		{
			continue;
		}
		else
		{
			spdlog::create_async_nb<spdlog::sinks::compressed_rotating_file_sink_mt>(it.first, it.second, rotated_max_size, rotated_max_files, compressed_max_files);
		}
	}
	spdlog::set_pattern("[%Y-%m-%d %T.%e] [%n] [%L] [%t] %v");
}

LOGWRAPPER_API void LogWrapper::Uninit()
{
	spdlog::shutdown();
}

LOGWRAPPER_API void LogWrapper::SetDefaultLogger(const std::string& logName)
{
	auto logger = GetLogger(logName);
	if (!logger)
	{
		return;
	}

	spdlog::set_default_logger(logger);
}

LOGWRAPPER_API std::string LogWrapper::GetDefaultLoggerName()
{
	static std::string name;
	name = spdlog::default_logger()->name();
	return name;
}

LOGWRAPPER_API void LogWrapper::SetLogLevel(const std::string& logName, LogType type)
{
	auto logger = GetLogger(logName);
	if (!logger)
	{
		return;
	}

	logger->set_level(GetSpdLogLevel(type));
}

LOGWRAPPER_API void LogWrapper::FlushLog(const std::string& logName)
{
	auto logger = GetLogger(logName);
	if (!logger)
	{
		return;
	}

	logger->flush();
}

LOGWRAPPER_API void LogWrapper::WriteLogA(const std::string& logName, LogType type, const char* log, ...)
{
	auto logger = GetLogger(logName);
	if (!logger)
	{
		return;
	}

	bool level_pass = logger->should_log(GetSpdLogLevel(type));
	if (!level_pass)
	{
		return;
	}

	va_list args;
	va_start(args, log);
	int len = _vscprintf(log, args) + 1;
	std::string text;
	text.resize((size_t)len);
	vsprintf_s((char*)text.data(), (size_t)len, log, args);
	va_end(args);
	text.resize((size_t)(len - 1));

	logger->log(GetSpdLogLevel(type), "{}", text);
}

LOGWRAPPER_API void LogWrapper::WriteLogA(const std::string& logName, LogType type, const char* log, va_list args)
{
	auto logger = GetLogger(logName);
	if (!logger)
	{
		return;
	}

	bool level_pass = logger->should_log(GetSpdLogLevel(type));
	if (!level_pass)
	{
		return;
	}

	int len = _vscprintf(log, args) + 1;
	std::string text;
	text.resize((size_t)len);
	vsprintf_s((char*)text.data(), (size_t)len, log, args);
	text.resize(len - 1);

	logger->log(GetSpdLogLevel(type), "{}", text);
}

LOGWRAPPER_API void LogWrapper::WriteLogW(const std::string& logName, LogType type, const wchar_t* log, ...)
{
	auto logger = GetLogger(logName);
	if (!logger)
	{
		return;
	}

	bool level_pass = logger->should_log(GetSpdLogLevel(type));
	if (!level_pass)
	{
		return;
	}
	
	va_list args;
	va_start(args, log);
	int len = _vscwprintf(log, args) + 1;
	std::wstring text;
	text.resize((size_t)len);
	vswprintf_s((wchar_t*)text.data(), (size_t)len, log, args);
	va_end(args);
	text.resize(len - 1);

	logger->log(GetSpdLogLevel(type), L"{}", text);
}

LOGWRAPPER_API void LogWrapper::WriteLogW(const std::string& logName, LogType type, const wchar_t* log, va_list args)
{
	auto logger = GetLogger(logName);
	if (!logger)
	{
		return;
	}

	bool level_pass = logger->should_log(GetSpdLogLevel(type));
	if (!level_pass)
	{
		return;
	}

	int len = _vscwprintf(log, args) + 1;
	std::wstring text;
	text.resize((size_t)len);
	vswprintf_s((wchar_t*)text.data(), (size_t)len, log, args);
	va_end(args);
	text.resize(len - 1);

	logger->log(GetSpdLogLevel(type), L"{}", text);
}

LOGWRAPPER_API std::wstring LogWrapper::GetLogPath(const std::string& logName)
{
	static std::wstring strLogPath;

	auto logger = GetLogger(logName);
	if (!logger || logger->sinks().empty())
	{
		strLogPath.clear();
		return strLogPath;
	}

	auto sink_ptr_ = logger->sinks().at(size_t(0));
	auto sink_ = sink_ptr_.get();

	if (!sink_)
	{
		strLogPath.clear();
		return strLogPath;
	}

	spdlog::sinks::compressed_rotating_file_sink_mt* my_sink = dynamic_cast<spdlog::sinks::compressed_rotating_file_sink_mt*>(sink_);
	strLogPath = my_sink->filename();
	return strLogPath;
}

class LogWrapper::CStopWatcherImpl
{
public:
	CStopWatcherImpl(const std::string& log)
	{
		m_stopWatcher = spdlog::stopwatch();
		m_logger = spdlog::default_logger();
		m_logA = log;
	}

	CStopWatcherImpl(const std::wstring& log)
	{
		m_stopWatcher = spdlog::stopwatch();
		m_logger = spdlog::default_logger();
		m_logW = log;
	}

	CStopWatcherImpl(const std::string& logName, const std::string& log)
	{
		m_stopWatcher = spdlog::stopwatch();
		m_logger = spdlog::get(logName);
		m_logA = log;
	};

	CStopWatcherImpl(const std::string& logName, const std::wstring& log)
	{
		m_stopWatcher = spdlog::stopwatch();
		m_logger = spdlog::get(logName);
		m_logW = log;
	};

	~CStopWatcherImpl()
	{
		if (m_logger)
		{
			if (!m_logA.empty())
			{
				m_logger->critical("{} Elapsed:{}", m_logA, std::chrono::duration_cast<std::chrono::milliseconds>(m_stopWatcher.elapsed()));
			}
			else if (!m_logW.empty())
			{
				m_logger->critical(L"{} Elapsed:{}", m_logW, std::chrono::duration_cast<std::chrono::milliseconds>(m_stopWatcher.elapsed()));
			}
			else
			{
				m_logger->critical("Elapsed:{}", std::chrono::duration_cast<std::chrono::milliseconds>(m_stopWatcher.elapsed()));
			}
		}
	};

private:
	spdlog::stopwatch m_stopWatcher;
	std::shared_ptr<spdlog::logger> m_logger;
	std::string m_logA;
	std::wstring m_logW;
};

LogWrapper::CStopWatcher::CStopWatcher(const std::string& log)
{
	m_impl = new CStopWatcherImpl(log);
}

LogWrapper::CStopWatcher::CStopWatcher(const std::wstring& log)
{
	m_impl = new CStopWatcherImpl(log);
}

LogWrapper::CStopWatcher::CStopWatcher(const std::string& logName, const std::string& log)
{
	m_impl = new CStopWatcherImpl(logName, log);
}

LogWrapper::CStopWatcher::CStopWatcher(const std::string& logName, const std::wstring& log)
{
	m_impl = new CStopWatcherImpl(logName, log);
}

LogWrapper::CStopWatcher::~CStopWatcher()
{
	delete m_impl;
	m_impl = nullptr;
}