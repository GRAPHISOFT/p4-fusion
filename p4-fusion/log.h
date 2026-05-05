/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

class Log
{
public:
	enum class LogLevel
	{
		Normal = 0,
		Verbose
	};

	enum class LogType
	{
		Print,
		Error,
		Warning,
		Success
	};

	/// @warning NOT thread-safe. Must be called from the main thread only,
	///          before any worker threads are spawned.
	static void SetLogLevel(LogLevel logLevel);

	/// @warning NOT thread-safe. Must be called from the main thread only,
	///          before any worker threads are spawned.
	static void DisableColoredOutput();

	/// @warning NOT thread-safe. Must be called from the main thread only,
	///          before any worker threads are spawned.
	static bool StartLogFile(const std::string& filePath);

	/// Thread-safe. Outputs to stderr or stdout based on LogType, and writes to the log file.
	static void DoLog(const std::string& message, LogType logType, LogLevel logLevel);

	/// Thread-safe.
	static std::string GetLogLineHeader(LogType logType, const char* func, int line);

private:
	static LogLevel CurrentLogLevel;

	static const char* ColorRed;
	static const char* ColorYellow;
	static const char* ColorGreen;
	static const char* ColorNormal;

	static std::ofstream logFileStream;
	static std::mutex logFileStreamMutex;

	/// Thread-safe.
	static std::string Timestamp();

	/// Thread-safe.
	static std::string ThreadName();

	/// Thread-safe.
	static void WriteToLogFile(const std::string& message);
};

#define PRINT(message)                                                                                         \
	do                                                                                                         \
	{                                                                                                          \
		std::ostringstream logMessageStream;                                                                   \
		logMessageStream << Log::GetLogLineHeader(Log::LogType::Print, __func__, __LINE__) << message << '\n'; \
		Log::DoLog(logMessageStream.str(), Log::LogType::Print, Log::LogLevel::Normal);                        \
	} while (0)

#define PRINT_VERBOSE(message)                                                                                 \
	do                                                                                                         \
	{                                                                                                          \
		std::ostringstream logMessageStream;                                                                   \
		logMessageStream << Log::GetLogLineHeader(Log::LogType::Print, __func__, __LINE__) << message << '\n'; \
		Log::DoLog(logMessageStream.str(), Log::LogType::Print, Log::LogLevel::Verbose);                       \
	} while (0)

#define ERR(message)                                                                                           \
	do                                                                                                         \
	{                                                                                                          \
		std::ostringstream logMessageStream;                                                                   \
		logMessageStream << Log::GetLogLineHeader(Log::LogType::Error, __func__, __LINE__) << message << '\n'; \
		Log::DoLog(logMessageStream.str(), Log::LogType::Error, Log::LogLevel::Normal);                        \
	} while (0)

#define WARN(message)                                                                                            \
	do                                                                                                           \
	{                                                                                                            \
		std::ostringstream logMessageStream;                                                                     \
		logMessageStream << Log::GetLogLineHeader(Log::LogType::Warning, __func__, __LINE__) << message << '\n'; \
		Log::DoLog(logMessageStream.str(), Log::LogType::Warning, Log::LogLevel::Normal);                        \
	} while (0)

#define SUCCESS(message)                                                                                         \
	do                                                                                                           \
	{                                                                                                            \
		std::ostringstream logMessageStream;                                                                     \
		logMessageStream << Log::GetLogLineHeader(Log::LogType::Success, __func__, __LINE__) << message << '\n'; \
		Log::DoLog(logMessageStream.str(), Log::LogType::Success, Log::LogLevel::Normal);                        \
	} while (0)
