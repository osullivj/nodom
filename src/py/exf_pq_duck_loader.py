# builtin
import fnmatch
import logging
import os
import os.path
# 3rd pty
import duckdb
import pandas as pd
import pyarrow as pa
import pyarrow.csv as pa_csv
import pyarrow.parquet as pq
# h3gui
import nd_consts
import nd_utils

# TODO
# argparse
# batch translate all depth

# one set per field: see how many distinct vals...
# invariant: data, sym, MessageType, Instance, Region, MarketName, DisplayName,
# variant: time
# https://stackoverflow.com/questions/48899051/how-to-drop-a-specific-column-of-csv-file-while-reading-it-using-pandas

IGNORE=['data', 'MessageType', 'Instance', 'Region', 'MarketName', 'DisplayName', 'SubMarketName'
           'InstrumentId', 'InstrumentMarketType', 'PriceType', 'WorkupPrice', 'WorkupSize',
           'BestBidOrders', 'BestAskOrders', 'BidLockPrice', 'AskLockPrice', 'FeedId']

# Source data has hh:mm:ss.fff
TIME_FIELDS = ['time', 'FeedCaptureTS']

strip_hms = lambda ts: pd.Timestamp(year=ts.year, month=ts.month, day=ts.day)


SQL="""DROP TABLE IF EXISTS depth;
CREATE TABLE depth AS SELECT * FROM read_parquet('*.parquet');
"""

if __name__ == '__main__':
    nd_utils.init_logging('pq_loader')
    source_files = nd_utils.parquet_files(nd_consts.PQ_DIR)
    logging.info(f'Source PQs found in {nd_consts.PQ_DIR}')
    logging.info(f'{source_files}')
    target_db = os.path.join(nd_consts.ND_ROOT_DIR, 'dat', 'depth.db')
    logging.info(f'parquet will aggregate in {target_db}')
    # DuckDB connection: set cwd to be parquet dir
    os.chdir(nd_consts.PQ_DIR)
    conn = duckdb.connect(target_db, read_only=False)
    sql = SQL
    logging.info(f'DuckDB load query: {sql}')
    conn.sql(sql)
    conn.commit()
    conn.close()

