#pragma once
#include <string>
#include <iostream>
#include <ostream>
#include <sstream>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

class NDOutBuffer : public std::stringbuf {
public:
	// https://en.cppreference.com/w/cpp/io/basic_streambuf/pubsync.html
	virtual int sync() {
#ifndef __EMSCRIPTEN__
		std::cout << this->str();
#else
		emscripten_log(EM_LOG_CONSOLE | EM_LOG_INFO, "%s", this->str().c_str());
#endif
		// clear buf by setting contents to empty string
		this->str("");
		return 0;	// success
	}
};

class NDErrBuffer : public std::stringbuf {
protected:
	virtual int sync() {
#ifndef __EMSCRIPTEN__
		std::cerr << this->str();
#else
		emscripten_log(EM_LOG_CONSOLE | EM_LOG_ERROR, "%s", this->str().c_str());
#endif
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
	NDOutBuffer		out_buffer;
	NDErrBuffer		err_buffer;
	std::ostream	out_stream{ &out_buffer };
	std::ostream	err_stream{ &err_buffer };

public:
	std::ostream& out() { return out_stream; }
	std::ostream& err() { return err_stream; }

	static NDLogger& getInstance() {
		// using the Scott Meyers singleton pattern
		static NDLogger instance;
		return instance;
	}

	static std::ostream& cout() {
		return NDLogger::getInstance().out();
	}

	static std::ostream& cerr() {
		return NDLogger::getInstance().err();
	}
};