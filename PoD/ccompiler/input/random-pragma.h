#pragma once

#if QUERY_RANDOM_CONFIG

#pragma  message 'RANDOM_SIZE' RANDOM_SIZE
#pragma  message 'RANDOM_REDUCE' RANDOM_REDUCE

#else

#if RANDOM_SIZE != AVAIL_RANDOM_SIZE
#error wrong RANDOM_SIZE
#endif
#if RANDOM_REDUCE != AVAIL_RANDOM_REDUCE
#error wrong RANDOM_REDUCE
#endif

#endif // QUERY_RANDOM_CONFIG
