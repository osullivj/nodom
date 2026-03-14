#pragma once
#include "json_ops.hpp"

// JSON aware types that build on JSON ops and ND types
template <typename JSON>
struct NDWidget {
    NDWidget() = delete;
    NDWidget(StringVec& weq_ids, StringIntMap& weq_map, const JSON& w)
        :widget_id(extract_entity_id<JSON>(weq_ids, weq_map, w, Static::widget_id_cs)),
        rname(extract_render_name<JSON>(w)),
        cspec(extract_cspec<JSON>(w))
    { }
    RenderMethod    rname{ EndRenderMethod };
    uint32_t        widget_id{ 0 }; // invalid ID
    const JSON& cspec;
};


