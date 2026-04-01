/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

class Log
{
public:
	static const char* ColorRed;
	static const char* ColorYellow;
	static const char* ColorGreen;
	static const char* ColorNormal;

	/// @warning NOT thread-safe. Must be called from the main thread only,
	///          before any worker threads are spawned.
	static void DisableColoredOutput();

	/// Thread-safe.
	static std::string Timestamp();

	/// Thread-safe.
	static std::string ThreadName();
};

#define PRINT(x)                                                                                        \
	do                                                                                                  \
	{                                                                                                   \
		std::ostringstream _oss;                                                                        \
		_oss << "[ " << Log::Timestamp() << " " << Log::ThreadName() << " PRINT @ " << __func__ << ":" << __LINE__ << " ] " << x << '\n';  \
		std::cout << _oss.str() << std::flush;                                                           \
	} while (0)

#define ERR(x)                                                                                                         \
	do                                                                                                                 \
	{                                                                                                                  \
		std::ostringstream _oss;                                                                                       \
		_oss << Log::ColorRed << "[ " << Log::Timestamp() << " " << Log::ThreadName() << " ERROR @ " << __func__ << ":" << __LINE__ << " ] " << x \
		     << Log::ColorNormal << '\n';                                                                              \
		std::cerr << _oss.str() << std::flush;                                                                          \
	} while (0)

#define WARN(x)                                                                                                          \
	do                                                                                                                   \
	{                                                                                                                    \
		std::ostringstream _oss;                                                                                         \
		_oss << Log::ColorYellow << "[ " << Log::Timestamp() << " " << Log::ThreadName() << " WARNING @ " << __func__ << ":" << __LINE__ << " ] "   \
		     << x << Log::ColorNormal << '\n';                                                                           \
		std::cerr << _oss.str() << std::flush;                                                                            \
	} while (0)

#define SUCCESS(x)                                                                                                       \
	do                                                                                                                   \
	{                                                                                                                    \
		std::ostringstream _oss;                                                                                         \
		_oss << Log::ColorGreen << "[ " << Log::Timestamp() << " " << Log::ThreadName() << " SUCCESS @ " << __func__ << ":" << __LINE__ << " ] "    \
		     << x << Log::ColorNormal << '\n';                                                                           \
		std::cerr << _oss.str() << std::flush;                                                                            \
	} while (0)
