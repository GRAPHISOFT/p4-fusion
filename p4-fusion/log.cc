/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "log.h"

#define COLOR_RED "\033[91m"
#define COLOR_YELLOW "\033[93m"
#define COLOR_GREEN "\033[32m"
#define COLOR_NORMAL "\033[0m"

const char* Log::ColorRed = COLOR_RED;
const char* Log::ColorYellow = COLOR_YELLOW;
const char* Log::ColorGreen = COLOR_GREEN;
const char* Log::ColorNormal = COLOR_NORMAL;

// NOT thread-safe: mutates shared static pointers without synchronization.
// Must be called from the main thread before any worker threads are spawned.
void Log::DisableColoredOutput()
{
	ColorRed = COLOR_NORMAL;
	ColorYellow = COLOR_NORMAL;
	ColorGreen = COLOR_NORMAL;
	ColorNormal = COLOR_NORMAL;
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