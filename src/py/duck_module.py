# std pkgs
import logging
import os.path
# nodom
import nd_consts
import nd_web
import nd_utils
# 3rd pty
import duckdb
import pyarrow  # imported for breadboard C++ access

# duck_module.py: yes, it's named after duck_module.js
# breadboard's pybind11 code loads this module so that
# duckDB is in process, but in another thread. The most
# similar process config we can get to a duckDB-wasm JS
# app while doing all C++ DearImGui

NDAPP='duck_module'

logr = nd_utils.init_logging(NDAPP)

class DuckService(object):
    def __init__(self):
        # overwrite the default msg_handlers set in base
        # as we don't need DataChange and DuckOp here; those
        # handlers are for a "real" backend serving a nodom
        # duck browser process. In this case we're replacing
        # the browser hosted DB with server hosted. Note also that
        # we're not supplying a DataChange handler, as breadboard
        # routes that to its in process DataChange handler.
        self.msg_handlers = dict(
            ParquetScan=self.scan,
            Query=self.query,
            Null=self.null,
        )
        # in mem DB instance: NB duckdb_wasm doesn't persist
        self.duck_conn = duckdb.connect()

    def request(self, msg):
        logr.info(f'DuckService.request: {msg}')
        handler = self.msg_handlers.get(msg['nd_type'], self.null)
        return handler(msg)

    def null(self):
        return [dict(nd_type="DuckInstance")]

    def scan(self, msg_dict):
        # parquet scans do not produce a result set
        # ergo diff handler...
        logr.info(f'duck_module.scan: {msg_dict}')
        try:
            self.duck_conn.execute(msg_dict["sql"])
        except duckdb.IOException as ex:
            logr.error(f'DuckService.on_scan: {ex}')
        response = [dict(nd_type='ParquetScanResult', query_id=msg_dict['query_id'])]
        logr.info(f'scan: {response}')
        return response

    def query(self, msg_dict):
        logr.info(f'on_query: {msg_dict}')
        # PyArrow may not be the most efficient way to
        # handle results. But we have to use arrow with
        # duckdb-wasm, and breadboard is a test bed, so
        # arrow it is. JOS 2025-02-28
        arrow_table = self.duck_conn.sql(msg_dict["sql"])
        response = [dict(nd_type='QueryResult', query_id=msg_dict['query_id'],
                        result=arrow_table.to_arrow_table())]
        logr.info(f'on_query: {response}')
        return response


service = DuckService()
