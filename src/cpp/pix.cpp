// heavily ifdeffed so we can build Breadboard without linking PIX
#ifdef USE_PIX
#include "Windows.h"
#include "pix3.h"
#endif

#include "nd_types.hpp"

// TODO: refactor pix.cpp for Chrome and 
// gen util funcs
const char* DuckTypeToString(DuckType dt) {
    switch (dt) {
    case DuckType::Int:
        return "Int";
    case DuckType::Float:
        return "Float";
    case DuckType::Timestamp:
        return "TS";
    case DuckType::Timestamp_s:
        return "TS_s";
    case DuckType::Timestamp_ms:
        return "TS_ms";
    case DuckType::Timestamp_us:
        return "TS_us";
    case DuckType::Timestamp_ns:
        return "TS_ns";
    case DuckType::Utf8:
        return "Utf8";
    }
    return "Unknown";
}

int DuckTypeToSize(DuckType dt) {
    switch (dt) {
    case DuckType::Int:
        return 4;
    case DuckType::Float:
    case DuckType::Timestamp:
    case DuckType::Timestamp_s:
    case DuckType::Timestamp_ms:
    case DuckType::Timestamp_us:
    case DuckType::Timestamp_ns:
    case DuckType::Utf8:
        return 8;
    }
    return 0;
}


#ifdef USE_PIX
uint32_t render_color = PIX_COLOR(255, 0, 125);
uint32_t dbase_color = PIX_COLOR(125, 0, 255);
#endif

void pix_init() {
#ifdef USE_PIX
	DWORD pix_capture_flags = PIX_CAPTURE_TIMING |
		PIX_CAPTURE_FUNCTION_SUMMARY |
		PIX_CAPTURE_CALLGRAPH;

	PIXCaptureParameters pix_capture_parms;
	pix_capture_parms.TimingCaptureParameters.CaptureGpuTiming = false;
	pix_capture_parms.TimingCaptureParameters.CaptureCallstacks = true;
	pix_capture_parms.TimingCaptureParameters.CaptureVirtualAllocEvents = true;
	pix_capture_parms.TimingCaptureParameters.CaptureHeapAllocEvents = true;
	pix_capture_parms.TimingCaptureParameters.CaptureXMemEvents = false;
	pix_capture_parms.TimingCaptureParameters.CapturePixMemEvents = false;
	pix_capture_parms.TimingCaptureParameters.CapturePageFaultEvents = false;
	pix_capture_parms.TimingCaptureParameters.CaptureVideoFrames = false;

	PIXBeginCapture(pix_capture_flags, &pix_capture_parms);
#endif
}

void pix_fini() {
#ifdef USE_PIX
	PIXEndCapture(false);
#endif
}

void pix_begin_render(int render_count) {
#ifdef USE_PIX
	PIXBeginEvent(render_color, "render %d", render_count);
#endif
}

void pix_begin_dbase() {
#ifdef USE_PIX
	PIXBeginEvent(dbase_color, "dbase");
#endif
}


void pix_end_event() {
#ifdef USE_PIX
	PIXEndEvent();
#endif
}

const wchar_t* PixReportTypeToString(PixReportType rt) {
    switch (rt) {
    case RenderFPS:
        return L"RenderFPS";
    case RenderPushPC:
        return L"RenderPushPC";
    case RenderPopPC:
        return L"RenderPopPC";
    case DBScans:
        return L"DBScans";
    case DBQueries:
        return L"DBQueries";
    case DBBatches:
        return L"DBBatches";
    }
    return L"Unknown";
}

void pix_report(PixReportType t, float val) {
#ifdef USE_PIX
	PIXReportCounter(PixReportTypeToString(t), val);
#endif
}
