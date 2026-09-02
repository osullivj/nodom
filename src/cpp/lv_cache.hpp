#pragma once
#include <iostream>
#include <queue>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <list>
#include "nd_types.hpp"
#include "static_strings.hpp"
#include "json_ops.hpp"
#include "config.hpp"


#ifndef __EMSCRIPTEN__

#include "nlohmann.hpp"     
#include <boost/thread.hpp>
#include <boost/atomic.hpp>
#else   // __EMSCRIPTEN__

#include <emscripten/val.h>

#endif  // __EMSCRIPTEN__

// LIVE should be a struct of arrays; each array is a field in a live record
// For example, Tiingo IEX mid mkt data supplies these fields...
// 
// type		timestamp	ticker		mid
// char[1]	TS/int64	char[8]		double
//
// Tiingo IEX TOB mkt data supplies these fields...
// 
// type		timestamp	tickid	ticker		bidsz	bid		mid		ask		asksz	ltrade	ltradesz
// char[1]	TS/int64	int64	char[8]		int32	double	double	double	int32	double	int32
//
// LiveCache RecordCount needs to be a config param

struct TiingoIEXMidRecords {
	uint32_t	record_count{ 0 };
	int64_t*	time_stamp{ nullptr };
	char*		ticker{ nullptr };	// 8 char ticker, so this is 64 like int and dbl
	double*		mid{ nullptr };

	TiingoIEXMidRecords(uint32_t rc) :
		record_count(rc) {
		// 8 bytes per firld for the 64 bit fields
		time_stamp = (int64_t*)malloc(rc * 8);
		ticker = (char*)malloc(rc * 8);
		mid = (double*)malloc(rc * 8);
	}
	~TiingoIEXMidRecords() {
		free(mid);
		free(ticker);
		free(time_stamp);
	}
};

struct TiingoIEXTopRecords {
	uint32_t	record_count{ 0 };
	int64_t*	time_stamp{ nullptr };
	int64_t*	tick_id{ nullptr };
	char*		ticker{ nullptr };	// 8 char ticker, so this is 64 like int and dbl
	double*		bid{ nullptr };
	double*		mid{ nullptr };
	double*		ask{ nullptr };
	double*		last_trade{ nullptr };
	uint32_t*	bid_sz{ nullptr };
	uint32_t*	ask_sz{ nullptr };
	uint32_t*	last_trade_sz{ nullptr };

	TiingoIEXTopRecords(uint32_t rc) :
					record_count(rc) {
		// 8 bytes per firld for the 64 bit fields
		time_stamp	= (int64_t*)malloc(rc * 8);
		tick_id		= (int64_t*)malloc(rc * 8);
		ticker		= (char*)malloc(rc * 8);
		bid			= (double*)malloc(rc * 8);
		mid			= (double*)malloc(rc * 8);
		ask			= (double*)malloc(rc * 8);
		last_trade	= (double*)malloc(rc * 8);
		bid_sz		= (uint32_t*)malloc(rc * 4);
		ask_sz		= (uint32_t*)malloc(rc * 4);
		last_trade_sz = (uint32_t*)malloc(rc * 4);
	}
	~TiingoIEXTopRecords() {
		free(last_trade_sz);
		free(ask_sz);
		free(bid_sz);
		free(last_trade);
		free(ask);
		free(mid);
		free(bid);
		free(ticker);
		free(tick_id);
		free(time_stamp);
	}
};


template <typename LIVE>
class LiveCache {
private:
	LIVE	live_records;

public:
	LiveCache(uint32_t rc)
		:live_records(rc) {
	}
};