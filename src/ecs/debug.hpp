#pragma once

#include <iostream>

// Unified debug output control for the entire project
#ifdef NDEBUG
#define DEBUG_LOG(x) do {} while(0)
#else
#define DEBUG_LOG(x) std::cout << x << std::endl
#endif