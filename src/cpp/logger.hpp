#pragma once
#include <string>
#include <iostream>
#include <ostream>
#include <sstream>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include "imgui.h"
#include "imgui_internal.h"
#else
#include <boost/thread.hpp>
#endif

// On Windows in Breadboard, we need to serialize access to the log
// Otherwise log lines get interleaved, making troubleshooting harder
// Leaning on the static init guarantee here...
#ifndef __EMSCRIPTEN__
static boost::mutex log_mutex;
#endif

class NDOutBuffer : public std::stringbuf {
private:
	std::string format;	// "NDI:%s" or "NDE:%s"
	bool		imgui_log{ false };
public:
	virtual ~NDOutBuffer() {}

	NDOutBuffer(const std::string& fmt, bool imlog=false)
		:format(fmt), imgui_log(imlog), std::stringbuf() { }

	void set_imgui_logging(bool imlog) {
		imgui_log = imlog;
	}
protected:
	// https://en.cppreference.com/w/cpp/io/basic_streambuf/pubsync.html
	virtual int sync() {
#ifndef __EMSCRIPTEN__
		boost::unique_lock<boost::mutex> log_lock(log_mutex);
		std::cout << this->str();
#else
		emscripten_log(EM_LOG_CONSOLE | EM_LOG_INFO, "%s", this->str().c_str());
#endif
		if (imgui_log) {
			ImGui::DebugLog(format.c_str(), this->str().c_str());
		}
		// clear buf by setting contents to empty string
		this->str("");
		return 0;	// success
	}
};

// Logger enables redirection from std::cout and
// std::cerr to emscripten_log(level, fmt, ...)
// Logger uses NDBuffer to capture std::cout/cerr
// https://stackoverflow.com/questions/13703823/a-custom-ostream
class NDLogger {
private:
	NDOutBuffer		info_buffer{Static::info_format_cs};
	NDOutBuffer		err_buffer{Static::err_format_cs};
	std::ostream	info_stream{ &info_buffer };
	std::ostream	err_stream{ &err_buffer };

public:
	std::ostream& out() { return info_stream; }
	std::ostream& err() { return err_stream; }

	static NDLogger& get_instance() {
		// using the Scott Meyers singleton pattern
		static NDLogger instance;
		return instance;
	}

	static std::ostream& cout() {
		return NDLogger::get_instance().out();
	}

	static std::ostream& cerr() {
		return NDLogger::get_instance().err();
	}

	void set_imgui_logging(bool imlog) {
		info_buffer.set_imgui_logging(imlog);
		err_buffer.set_imgui_logging(imlog);
	}
};