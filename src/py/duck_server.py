# std pkgs
import asyncio
import logging
import os.path
# tornado
import tornado
import tornado.websocket
from tornado.options import define, options, parse_command_line
from tornado.web import StaticFileHandler
import duckdb
# nodom
import nd_consts
import nd_web
import nd_utils

# duck_server is used as a child process by breadboard as
# a standin for duckdb-wasm in browser
NDAPP='duck_server'

logr = nd_utils.init_logging(NDAPP)

define("port", default=8892, help="run on the given port", type=int)

class DuckService(nd_utils.Service):
    def __init__(self):
        super().__init__(NDAPP, {}, {})
        # overwrite the default msg_handlers set in base
        # as we don't need DataChange and DuckOp here; those
        # handlers are for a "real" backend serving a nodom
        # duck browser process. In this case we're replacing
        # the browser hosted DB with server hosted. Note also that
        # we're not supplying a DataChange handler, as breadboard
        # routes that to its in process DataChange handler.
        self.msg_handlers = dict(
            ParquetScan=self.on_scan,
            Query=self.on_query,
            DuckOp=self.on_duck_op,
        )
        # in mem DB instance: NB duckdb_wasm doesn't persist
        self.duck_conn = duckdb.connect()
        self.websocks = dict()

    def get_ws_handlers(self):
        # nd_utils.Service.get_ws_handlers gives us DataChange and DuckOp
        # that's appropriate for a "real" backend like add_server or
        # exf_server: they have to handle DataChange in the server side
        # cache, and they want to log DB ops for diagnostics. We'll impl
        # DuckOp too, but give DataChange a nullop as we're running with
        # breadboard which handles DataChange via NDServer pybind11 dispatch
        return dict(
            DuckOp=self.on_duck_op,
            DataChange=self.on_data_change_nullop,
            ParquetScan=self.on_scan,
            Query=self.on_query,
        )

    def send_duck_instance(self, uuid):
        logr.info(f'DuckService.send_duck_instance: {uuid}')
        websock = self.websocks.get(uuid)
        if websock:
            websock.write_message(dict(nd_type='DuckIns'
                                               'tance', uuid=uuid))
        else:
            logr.error(f'DuckService.send_duck_instance: bad uuid {uuid}')

    def on_data_change_nullop(self):
        logr.info(f'DuckService.null')

    def on_ws_open(self, websock):
        self.websocks[websock._uuid] = websock
        tornado.ioloop.IOLoop.current().add_callback(self.send_duck_instance, websock._uuid)

    def on_scan(self, uuid, msg_dict):
        # parquet scans do not produce a result set
        # ergo diff handler...
        logr.info(f'on_scan: {uuid} {msg_dict}')
        try:
            self.duck_conn.execute(msg_dict["sql"])
        except duckdb.IOException as ex:
            logr.error(f'DuckService.on_scan: {ex}')
        logr.info(f'on_scan: complete')
        return [dict(nd_type='ParquetScanResult', query_id=msg_dict['query_id'])]

    def on_query(self, uuid, msg_dict):
        logr.info(f'on_query: {uuid} {msg_dict}')
        # PyArrow may not be the most efficient way to
        # handle results. But we have to use arrow with
        # duckdb-wasm, and breadboard is a test bed, so
        # arrow it is. JOS 2025-02-28
        arrow_table = self.duck_conn.sql(msg_dict["sql"])
        logr.info(f'on_query: {rs}')
        return [dict(nd_type='QueryResult', query_id=msg_dict['query_id'])]


service = DuckService()

async def main():
    parse_command_line()
    app = nd_web.NDApp(service)
    app.listen(options.port)
    logr.info(f'{NDAPP} port:{options.port}')
    await asyncio.Event().wait()


if __name__ == "__main__":
    asyncio.run(main())
