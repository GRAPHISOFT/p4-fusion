/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
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

	static const char* ColorRed;
	static const char* ColorYellow;
	static const char* ColorGreen;
	static const char* ColorNormal;

	static LogLevel CurrentLogLevel;

	/// @warning NOT thread-safe. Must be called from the main thread only,
	///          before any worker threads are spawned.
	static void DisableColoredOutput();

	/// @warning NOT thread-safe. Must be called from the main thread only,
	///          before any worker threads are spawned.
	static bool StartLogFile(const std::string& filePath);

	/// Thread-safe.
	static void WriteToLogFile(const std::string& message);

	/// Thread-safe.
	static std::string Timestamp();

	/// Thread-safe.
	static std::string ThreadName();

	/// Thread-safe.
	static std::string GetLogLineHeader(const char* logType, const char* func, int line);

private:
	static std::ofstream logFileStream;
	static std::mutex logFileStreamMutex;
};

#define PRINT(message) PRINT_WITH_LEVEL(Log::LogLevel::Normal, message)

#define PRINT_VERBOSE(message) PRINT_WITH_LEVEL(Log::LogLevel::Verbose, message)

#define PRINT_WITH_LEVEL(logLevel, message)                                                        \
	do                                                                                             \
	{                                                                                              \
		std::ostringstream logMessageStream;                                                       \
		logMessageStream << Log::GetLogLineHeader("PRINT", __func__, __LINE__) << message << '\n'; \
		if (logLevel <= Log::CurrentLogLevel)                                                      \
			std::cout << logMessageStream.str() << std::flush;                                     \
		Log::WriteToLogFile(logMessageStream.str());                                               \
	} while (0)

#define ERR(message)                                                                                          \
	do                                                                                                        \
	{                                                                                                         \
		std::ostringstream logMessageStream;                                                                  \
		logMessageStream << Log::GetLogLineHeader("ERROR", __func__, __LINE__) << message << '\n';            \
		const std::string logMessage = logMessageStream.str();                                                \
		std::cerr << (std::string(Log::ColorRed) + logMessage + std::string(Log::ColorNormal)) << std::flush; \
		Log::WriteToLogFile(logMessage);                                                                      \
	} while (0)

#define WARN(message)                                                                                            \
	do                                                                                                           \
	{                                                                                                            \
		std::ostringstream logMessageStream;                                                                     \
		logMessageStream << Log::GetLogLineHeader("WARNING", __func__, __LINE__) << message << '\n';             \
		const std::string logMessage = logMessageStream.str();                                                   \
		std::cerr << (std::string(Log::ColorYellow) + logMessage + std::string(Log::ColorNormal)) << std::flush; \
		Log::WriteToLogFile(logMessage);                                                                         \
	} while (0)

#define SUCCESS(message)                                                                                        \
	do                                                                                                          \
	{                                                                                                           \
		std::ostringstream logMessageStream;                                                                    \
		logMessageStream << Log::GetLogLineHeader("SUCCESS", __func__, __LINE__) << message << '\n';            \
		const std::string logMessage = logMessageStream.str();                                                  \
		std::cerr << (std::string(Log::ColorGreen) + logMessage + std::string(Log::ColorNormal)) << std::flush; \
		Log::WriteToLogFile(logMessage);                                                                        \
	} while (0)
