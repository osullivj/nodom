# builtin
import logging
import os
import os.path

import duckdb

# h3gui
import nd_consts
import nd_utils

# 3rd pty
import pandas as pd
import pyarrow.csv as pa_csv
import pyarrow.parquet as pq

logr = nd_utils.init_logging("pq_writer", console=True)

strip_hms = lambda ts: pd.Timestamp(year=ts.year, month=ts.month, day=ts.day)


def fix_trandatetime(source_dir, target_dir, csv_file):
    csv_in_path = os.path.join(source_dir, csv_file)
    csv_base, csv_ext = os.path.splitext(csv_file)
    csv_out_name = f"{csv_base}_pd{csv_ext}"
    csv_base = f"{csv_base}_pd"
    csv_out_path = os.path.join(target_dir, csv_out_name)
    df = pd.read_csv(
        csv_in_path,
        usecols=nd_consts.COLUMNS.keys(),
        parse_dates=list(nd_consts.RAW_DATE_FORMATS.keys()),
        date_format=nd_consts.RAW_DATE_FORMATS,
    )
    logging.info("== df.dtypes 1")
    logging.info(df.dtypes)
    logging.info(df.shape)
    # add ".000" suffix to LastTradeTime
    df["LastTradeTime"] = df["LastTradeTime"] + ".000"
    df.to_csv(csv_out_path, columns=nd_consts.COLUMNS, index=False)
    return csv_base, csv_out_path


# pyarrow has problems with parsing date formats direct from CSV
# https://stackoverflow.com/questions/73780443/pyarrow-issue-with-timestamp-data
def write_parquet_arrow(base_name, csv_in_path, target_dir):
    colnames = list(nd_consts.COLUMNS.keys())
    # skip_rows=1 to ignore col headers as we specify the col names
    # and types, and have autogenerate_column_names=False
    read_options = pa_csv.ReadOptions(
        column_names=colnames, autogenerate_column_names=False, skip_rows=1
    )
    convert_options = pa_csv.ConvertOptions(
        include_columns=colnames,
        column_types=dict(
            zip(colnames, map(lambda x: x[1], nd_consts.COLUMNS.values()))
        ),
        include_missing_columns=False,
        strings_can_be_null=False,
    )
    table = pa_csv.read_csv(
        csv_in_path, read_options=read_options, convert_options=convert_options
    )
    pq_out_path = os.path.join(target_dir, f"{base_name}.parquet")
    logr.info(f"== writing {pq_out_path}")
    logr.info(f"columns:{table.column_names}")
    logr.info(f"schema:{table.schema}")
    pq.write_table(table, pq_out_path)
    metadata = pq.read_metadata(pq_out_path)
    logr.info(f"metadata:{metadata}")
    return pq_out_path


# currently unused: we may revert to using Duck CSV processing in future
def write_parquet_duck(base_name, csv_in_path, target_dir):
    colnames = list(nd_consts.COLUMNS.keys())
    pq_out_path = os.path.join(target_dir, f"{base_name}.parquet")
    read_csv_args = dict(
        header=True,
        columns=nd_consts.COLUMNS,
        date_format=nd_consts.DATE_FMT,
        timestamp_format=nd_consts.DATETIME_FMT,
    )
    rel = duckdb.read_csv(csv_in_path, **read_csv_args)
    rel.write_parquet(pq_out_path)


if __name__ == "__main__":
    # nd_utils.init_logging(__file__)
    # use pandas to normalize timestamp formats
    source_files = nd_utils.file_list(nd_consts.PQ_DIR, "FGB??8_200809[012]?.csv")
    logr.info(f"Source CSVs found in {nd_consts.PQ_DIR}\n{source_files}")
    for sf in source_files:
        source_path = os.path.join(nd_consts.PQ_DIR, sf)
        base_name, csv_out_path = fix_trandatetime(
            nd_consts.PQ_DIR, nd_consts.PQ_DIR, sf
        )
        pq_out_path = write_parquet_arrow(base_name, csv_out_path, nd_consts.PQ_DIR)
        logr.info(f"{pq_out_path} written")
