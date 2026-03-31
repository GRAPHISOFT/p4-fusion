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

	static void DisableColoredOutput();

	static std::string Timestamp();
};

#define PRINT(x) std::cout << "[ " << Log::Timestamp() << " PRINT @ " << __func__ << ":" << __LINE__ << " ] " << x << std::endl

#define ERR(x)                                                                              \
	std::cerr << Log::ColorRed                                                              \
	          << "[ " << Log::Timestamp() << " ERROR @ " << __func__ << ":" << __LINE__ << " ] " \
	          << x << Log::ColorNormal << std::endl

#define WARN(x) std::cerr << Log::ColorYellow                                                         \
	                      << "[ " << Log::Timestamp() << " WARNING @ " << __func__ << ":" << __LINE__ << " ] " \
	                      << x << Log::ColorNormal << std::endl

#define SUCCESS(x) std::cerr << Log::ColorGreen                                                          \
	                         << "[ " << Log::Timestamp() << " SUCCESS @ " << __func__ << ":" << __LINE__ << " ] " \
	                         << x << Log::ColorNormal << std::endl
