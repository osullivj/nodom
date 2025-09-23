# std pkgs
import asyncio
import functools
import json
import logging
import os.path
# tornado
import tornado.websocket
from tornado.options import options, parse_command_line, define
# nodom
import nd_web
import nd_utils
import nd_consts

NDAPP='exf_server'

logr = nd_utils.init_logging(NDAPP)

# unique strings; single point of definition for query and button
# IDs used in action dispatch as these are used in layout and data
# and must be consistent. JOS 2025-02-06
SCAN_QID='depth_scan'
SELECT_QID='depth_query'
SUMMARY_QID='depth_summary'
SCAN_BUTTON_TEXT='Scan'
SUMMARY_BUTTON_TEXT='Summary'

EXF_LAYOUT = [
    dict(
        rname='Home',
        cspec=dict(
            title='Eurex Futures',
            # only applicable here in the Home widget
            gui_canvas_style_width="200px",
            gui_canvas_style_height="100px",
            shell_canvas_style_left="100px",
            shell_canvas_style_top = "0px"
        ),
        children=[
            dict(
                rname='Combo',
                cspec=dict(
                    cname='instruments',
                    index='selected_instrument',
                    label='Instrument',
                ),
            ),
            # see src/imgui.ts for enum defns
            # TableFlags.ReadOnly == 1 << 14 == 16384
            dict(
                rname='DatePicker',
                cspec=dict(
                    cname='start_date',
                    table_flags=0,
                    table_size=(280, -1),
                ),
            ),
            dict(rname='SameLine'),
            dict(rname='Text',
                cspec=dict(
                    text='Start date',
                ),
            ),
            dict(
                rname='DatePicker',
                cspec=dict(
                    cname='end_date',
                    table_flags=0,
                ),
            ),
            dict(rname='SameLine'),
            dict(rname='Text',
                cspec=dict(
                    text='End date',
                ),
            ),
            dict(rname='Separator', cspec=dict()),
            dict(
                rname='Button',
                cspec=dict(
                    text=SCAN_BUTTON_TEXT,
                ),
            ),
            dict(rname='SameLine'),
            dict(
                rname='Button',
                cspec=dict(
                    text=SUMMARY_BUTTON_TEXT,
                ),
            ),
            dict(rname='Separator', cspec=dict()),
            dict(rname='Table', cspec=dict(title='Depth grid',cname='depth_query_result')),
            dict(rname='Footer', cspec=dict(db=True, fps=True, demo=True, id_stack=True, memory=True)),
        ],
    ),
    # not a Home child? Must have an ID to be pushable
    dict(
        widget_id='depth_summary_modal',
        rname='DuckTableSummaryModal',
        cspec=dict(
            title='Depth table',
            cname='depth_summary_result',
        ),
    ),
    dict(
        widget_id='parquet_loading_modal',
        rname='DuckParquetLoadingModal',
        cspec=dict(
            title='Loading parquet...',
            cname='scan_urls',
        ),
    ),
]

SCAN_SQL = 'BEGIN; DROP TABLE IF EXISTS depth; CREATE TABLE depth as select * from parquet_scan(%(scan_urls)s); COMMIT;'
DEPTH_SQL = 'select * from depth where SeqNo > 0 order by CaptureTS limit 10 offset %(depth_offset)s;'
SUMMARY_SQL = 'summarize select * from depth;'

EXF_DATA = dict(
    home_title = 'FGB',
    start_date = (2008,9,1),       # 3 tuple YMD
    end_date = (2008,9,1),
    # NB tuple gives us Array in TS, and list gives us Object
    instruments = ('FGBMU8', 'FGBMZ8', 'FGBXZ8', 'FGBSU8', 'FGBSZ8', 'FGBXU8', 'FGBLU8', 'FGBLZ8'),
    selected_instrument = 0,
    # depth scan SQL, ID and URLs
    scan_sql = SCAN_SQL % dict(scan_urls=[]),
    scan_urls = [],
    # depth query SQL and ID
    depth_sql = DEPTH_SQL % dict(depth_offset=0),
    summary_sql = SUMMARY_SQL,
    # empty placeholder: see main.ts:on_duck_event for hardwiring of db_summary_ prefix
    db_summary_depth = dict(),
    depth_tick = dict(),
    depth_offset = 0,
    depth_results = nd_consts.EMPTY_TABLE,
    actions = {
        # match on scan button (SCAN_BUTTON_TEXT) click (Button)
        SCAN_BUTTON_TEXT:dict(
            # raise scanning modal and send scan request
            # to duckDB when Scan
            nd_events=["Button"],          # fired from Scan button
            ui_push="parquet_loading_modal",
            db=dict(
                action='ParquetScan',
                sql_cname='scan_sql',
                query_id=SCAN_QID,
            ),
        ),
        # match on completion (ParquetScanResult) of scan (SCAN_QID)
        # summary of depth table
        SCAN_QID:dict(
            nd_events=["ParquetScanResult"],
            ui_pop="DuckParquetLoadingModal",
            db=dict(
                action='Query',
                sql_cname='summary_sql',
                query_id=SUMMARY_QID,
            )
        ),
        # match on completion (QueryResult) of summary query (SUMMARY_QID)
        # the query will prime the depth data
        SUMMARY_QID:dict(
        # "disable":dict(
            nd_events=["QueryResult"],
            db=dict(
                action='Query',
                sql_cname='depth_sql',
                query_id=SELECT_QID,
            )
        ),
        SUMMARY_BUTTON_TEXT:dict(
            nd_events=["Button"],
            # depth_summary_modal is self popping, so
            # no need for a corresponding ui_pop like
            # parquet_loading_modal
            ui_push="depth_summary_modal",
        )
    }
)

# nd_utils.file_list needs one more arg after this partial bind for the pattern we're matching
parquet_list_func = functools.partial(nd_utils.file_list, nd_consts.PQ_DIR, '*.parquet')


EXTRA_HANDLERS = [
    (r'/api/parquet/(.*)', nd_web.ParquetHandler, dict(path=os.path.join(nd_consts.ND_ROOT_DIR, 'dat')))
]

is_scan_change = lambda c: c.get('cache_key') in ['start_date', 'end_date', 'selected_instrument']
is_depth_offset_change = lambda c: c.get('cache_key') == 'depth_offset'

class DepthService(nd_utils.Service):
    def on_client_data_change(self, uuid, client_change):
        logr.info(f'on_client_data_change:client:{uuid}, change:{client_change}')
        data_cache = self.cache['data']
        # Have selected_instrument, start_date or end_date changed?
        # If so we need to send a fresh parquet_scan up to the client
        if is_scan_change(client_change):
            # first we need the selected instrument to compose a fmt string for
            # file name date matching
            instrument_index = data_cache['selected_instrument']
            instrument_name = data_cache['instruments'][instrument_index]
            # get a list of all files for this instrument: NB the * in the
            # match string, which is not a regex, it's a unix fnmatch
            instrument_specific_files = nd_utils.file_list(nd_consts.PQ_DIR, f'{instrument_name}_*.parquet')
            # reduce the list to only files in the date range
            # here the format string is a strftime format
            ranged_matches = nd_utils.date_ranged_file_name_matches(instrument_specific_files,
                            data_cache['start_date'], data_cache['end_date'], f'{instrument_name}_%Y%m%d.parquet')
            # convert filenames to PQ URLs
            old_urls_val = data_cache['scan_urls']
            new_urls_val = [f'https://localhost/api/parquet/{pqfile}' for pqfile in ranged_matches]
            data_cache['scan_urls'] = new_urls_val
            old_sql_val = data_cache['scan_sql']
            new_sql_val = SCAN_SQL % data_cache
            data_cache['scan_sql'] = new_sql_val
            # finally, return the extra changes to be processed by the client
            return [dict(nd_type='DataChange', cache_key='scan_sql', old_value=old_sql_val, new_value=new_sql_val),
                    dict(nd_type='DataChange', cache_key='scan_urls', old_value=old_urls_val, new_value=new_urls_val)]
        elif is_depth_offset_change(client_change):
            # front end has hit fwd or back button, so recompose depth query
            old_depth_sql = data_cache['depth_sql']
            new_depth_sql = DEPTH_SQL % data_cache
            data_cache['depth_sql'] = new_depth_sql
            return [dict(nd_type='DataChange', cache_key='depth_sql', old_value=old_depth_sql, new_value=new_depth_sql)]


# for security reasons duck only ingests parquet via 443
define("port", default=443, help="run on the given port", type=int)

# breadboard looks out for service at the module level
# NB 4th param for duck app
service = DepthService(NDAPP, EXF_LAYOUT, EXF_DATA, True)

async def main():
    parse_command_line()
    cert_path = os.path.normpath(os.path.join(nd_consts.ND_ROOT_DIR, 'cfg', 'h3'))
    app = nd_web.NDApp(service, EXTRA_HANDLERS)
    https_server = tornado.httpserver.HTTPServer(app, ssl_options={
        "certfile": os.path.join(cert_path, "ssl_cert.pem"),
        "keyfile": os.path.join(cert_path, "ssl_key.pem"),
    })
    https_server.listen(options.port)
    logr.info(f'{NDAPP} port:{options.port} cert_path:{cert_path}')
    await asyncio.Event().wait()


if __name__ == "__main__":
    asyncio.run(main())
