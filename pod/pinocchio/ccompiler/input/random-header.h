#pragma once

#if BIT_WIDTH==32
#include "random-header-32.h"
#elif BIT_WIDTH==110
#include "random-header-110.h"
#else
#error Undefined BIT_WIDTH
#endif
