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

template <typename JSON>
struct TiingoIEXMidRecords {
	uint32_t	record_count{ 0 };
	int64_t*	time_stamp{ nullptr };
	char*		ticker{ nullptr };	// 8 char ticker, so this is 64 like int and dbl
	double*		mid{ nullptr };

	TiingoIEXMidRecords() { }

	~TiingoIEXMidRecords() {
		free(mid);
		free(ticker);
		free(time_stamp);
	}

	void init(uint32_t rc) {
		record_count = rc;
		// 8 bytes per firld for the 64 bit fields
		time_stamp = (int64_t*)malloc(rc * 8);
		ticker = (char*)malloc(rc * 8);
		mid = (double*)malloc(rc * 8);
		memset(time_stamp, 0, rc * 8);
		memset(ticker, 0, rc * 8);
		memset(mid, 0, rc * 8);
	}

	bool update(uint32_t inx, const std::string& tckr, const JSON& upd) {
		if (inx >= record_count)
			return false;
		assert(JContains(upd, Static::mid_cs));
		mid[inx] = JAsDouble(upd, Static::mid_cs);
		time_stamp[inx] = JAsDouble(upd, Static::timestamp_cs);
		return true;
	}
};



template <typename JSON, typename LIVE>
class LiveCache {
private:
	LIVE		records;

	// working storage
	std::string ticker;
	StringVec	ticker_list;
	char*		ticker_ptr{ nullptr };
	uint32_t	subbed_count{ 0 };
	uint32_t	ticker_inx{ 0 };

	char* find_free_ticker(uint32_t& inx) {
		inx = 0;
		ticker_ptr = records.ticker;
		// wind fwd til we find a free slot
		while (*ticker_ptr != 0 && inx < records.record_count) {
			ticker_ptr += 8;
			inx++;
		}
		if (inx >= records.record_count) {
			return nullptr;
		}
		return ticker_ptr;
	}

	bool find_ticker(const char* t, uint32_t& inx) {
		inx = 0;
		// wind fwd til we find a match
		while (inx < records.record_count) {
			ticker_ptr = records.ticker + (inx * 8);
			if (std::string_view(ticker_ptr) == std::string_view(t))
				return true;
			inx++;
		}
		return false;
	}



public:
	LiveCache( ) { }

	void init(uint32_t rc) {
		records.init(rc);
	}

	uint32_t on_sub(const JSON& resp) {
		assert(JContains(resp, Static::tickers_cs));
		ticker_list.clear();
		JAsStringVec(resp, Static::tickers_cs, ticker_list);
		ticker_inx = 0;
		subbed_count = 0;
		if (ticker_list.empty())
			return subbed_count;
		// TODO: this will not work for unsub/resub
		// NB only good for init one shot sub
		while (subbed_count < ticker_list.size()) {
			ticker_ptr = find_free_ticker(ticker_inx);
			if (ticker_ptr == nullptr || ticker_inx >= records.record_count)
				return subbed_count;
			assert(strlen(ticker_list[subbed_count].c_str()) < 8);
			if (ticker_ptr != nullptr) {	// found a free slot
				strncpy(ticker_ptr, ticker_list[subbed_count++].c_str(), 8);
			}
		}
		return subbed_count;
	}

	bool update(const JSON& upd) {
		assert(JContains(upd, Static::ticker_cs));
		ticker = JAsString(upd, Static::ticker_cs);
		if (find_ticker(ticker.c_str(), ticker_inx)) {
			return records.update(ticker_inx, ticker, upd);
		}
		return false;
	}

	uint32_t ticker_count() {
		subbed_count = 0;
		ticker_ptr = records.ticker;
		// wind fwd counting busy slots
		for (ticker_inx = 0; ticker_inx < records.record_count; ticker_inx++) {
			if (*ticker_ptr != 0)
				subbed_count++;
			ticker_ptr += 8;
		}
		return subbed_count;
	}
};

// TODO: impl Tiingo TOB
struct TiingoIEXTopRecords {
	uint32_t	record_count{ 0 };
	int64_t* time_stamp{ nullptr };
	int64_t* tick_id{ nullptr };
	char* ticker{ nullptr };	// 8 char ticker, so this is 64 like int and dbl
	double* bid{ nullptr };
	double* mid{ nullptr };
	double* ask{ nullptr };
	double* last_trade{ nullptr };
	uint32_t* bid_sz{ nullptr };
	uint32_t* ask_sz{ nullptr };
	uint32_t* last_trade_sz{ nullptr };

	void init(uint32_t rc) {
		record_count = rc;
		// 8 bytes per firld for the 64 bit fields
		time_stamp = (int64_t*)malloc(rc * 8);
		tick_id = (int64_t*)malloc(rc * 8);
		ticker = (char*)malloc(rc * 8);
		bid = (double*)malloc(rc * 8);
		mid = (double*)malloc(rc * 8);
		ask = (double*)malloc(rc * 8);
		last_trade = (double*)malloc(rc * 8);
		bid_sz = (uint32_t*)malloc(rc * 4);
		ask_sz = (uint32_t*)malloc(rc * 4);
		last_trade_sz = (uint32_t*)malloc(rc * 4);

		memset(time_stamp, 0, rc * 8);
		memset(tick_id, 0, rc * 8);
		memset(ticker, 0, rc * 8);
		memset(bid, 0, rc * 8);
		memset(mid, 0, rc * 8);
		memset(ask, 0, rc * 8);
		memset(last_trade, 0, rc * 8);
		memset(bid_sz, 0, rc * 8);
		memset(ask_sz, 0, rc * 8);
		memset(last_trade_sz, 0, rc * 8);
	}
	TiingoIEXTopRecords() {}

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