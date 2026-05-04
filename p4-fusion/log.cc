/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "log.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <thread>
#include <unordered_map>

#define COLOR_RED "\033[91m"
#define COLOR_YELLOW "\033[93m"
#define COLOR_GREEN "\033[32m"
#define COLOR_NORMAL "\033[0m"

const char* Log::ColorRed = COLOR_RED;
const char* Log::ColorYellow = COLOR_YELLOW;
const char* Log::ColorGreen = COLOR_GREEN;
const char* Log::ColorNormal = COLOR_NORMAL;

Log::LogLevel Log::CurrentLogLevel = Log::LogLevel::Normal;

std::ofstream Log::logFileStream;
std::mutex Log::logFileStreamMutex;

// NOT thread-safe: mutates shared static pointers without synchronization.
// Must be called from the main thread before any worker threads are spawned.
void Log::DisableColoredOutput()
{
	ColorRed = COLOR_NORMAL;
	ColorYellow = COLOR_NORMAL;
	ColorGreen = COLOR_NORMAL;
	ColorNormal = COLOR_NORMAL;
}

bool Log::StartLogFile(const std::string& filePath)
{
	logFileStream.open(filePath, std::ios::out | std::ios::app);
	return logFileStream.is_open();
}

void Log::Logging(const std::string& message, Log::LogType logType, Log::LogLevel logLevel)
{
	switch (logType)
	{
	case LogType::Error:
		std::cerr << (std::string(ColorRed) + message + std::string(ColorNormal)) << std::flush;
		break;
	case LogType::Warning:
		std::cerr << (std::string(ColorYellow) + message + std::string(ColorNormal)) << std::flush;
		break;
	case LogType::Success:
		std::cerr << (std::string(ColorGreen) + message + std::string(ColorNormal)) << std::flush;
		break;
	case LogType::Print:
		if (logLevel <= CurrentLogLevel)
		{
			std::cout << message << std::flush;
		}
		break;
	}

	WriteToLogFile(message);
}

std::string Log::GetLogLineHeader(Log::LogType logType, const char* func, int line)
{
	const char* logTypeStr = "";

	switch (logType)
	{
	case LogType::Print:
		logTypeStr = "PRINT";
		break;
	case LogType::Error:
		logTypeStr = "ERROR";
		break;
	case LogType::Warning:
		logTypeStr = "WARNING";
		break;
	case LogType::Success:
		logTypeStr = "SUCCESS";
		break;
	}

	return "[ " + Timestamp() + " " + ThreadName() + " " + logTypeStr + " @ " + func + ":" + std::to_string(line) + " ] ";
}

std::string Log::Timestamp()
{
	const auto now = std::chrono::system_clock::now();
	const auto time = std::chrono::system_clock::to_time_t(now);
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::tm tm;
	localtime_r(&time, &tm);

	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();

	return oss.str();
}

std::string Log::ThreadName()
{
	static std::mutex mtx;
	static std::unordered_map<std::thread::id, int> ids;
	static int nextId = 0;

	const auto threadID = std::this_thread::get_id();
	std::lock_guard<std::mutex> lock(mtx);
	auto it = ids.find(threadID);
	if (it == ids.end())
		it = ids.emplace(threadID, nextId++).first;

	return "Thread_" + (it->second == 0 ? "Main" : std::to_string(it->second));
}

void Log::WriteToLogFile(const std::string& message)
{
	std::lock_guard<std::mutex> lock(logFileStreamMutex);
	if (logFileStream.is_open())
	{
		logFileStream << message << std::flush;
	}
}