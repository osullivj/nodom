# builtin
import logging
import os
import os.path
# nodom
import nd_consts
import nd_utils
# 3rd pty
import duckdb
import pandas as pd
import pyarrow.csv as pa_csv
import pyarrow.parquet as pq

logr = nd_utils.init_logging("pq_writer", console=True)

strip_hms = lambda ts: pd.Timestamp(year=ts.year, month=ts.month, day=ts.day)

EXF_COLUMNS = dict(
    SeqNo=("INTEGER", pa.int32()),
    LastTradeTime=("TIMESTAMP", pa.timestamp("ms")),
    LastTradePrice=("DOUBLE", pa.float64()),
    LastTradeSize=("INTEGER", pa.int32()),
    HighPrice=("DOUBLE", pa.float64()),
    LowPrice=("DOUBLE", pa.float64()),
    Volume=("INTEGER", pa.int32()),
    LastTradeSequence=("INTEGER", pa.int32()),
    TranDateTime=("TIMESTAMP", pa.timestamp("ms")),
    BidQty1=("INTEGER", pa.int32()),
    AskQty1=("INTEGER", pa.int32()),
    BidQty2=("INTEGER", pa.int32()),
    AskQty2=("INTEGER", pa.int32()),
    BidQty3=("INTEGER", pa.int32()),
    AskQty3=("INTEGER", pa.int32()),
    BidQty4=("INTEGER", pa.int32()),
    AskQty4=("INTEGER", pa.int32()),
    BidQty5=("INTEGER", pa.int32()),
    AskQty5=("INTEGER", pa.int32()),
    BidPrice1=("DOUBLE", pa.float64()),
    AskPrice1=("DOUBLE", pa.float64()),
    BidPrice2=("DOUBLE", pa.float64()),
    AskPrice2=("DOUBLE", pa.float64()),
    BidPrice3=("DOUBLE", pa.float64()),
    AskPrice3=("DOUBLE", pa.float64()),
    BidPrice4=("DOUBLE", pa.float64()),
    AskPrice4=("DOUBLE", pa.float64()),
    BidPrice5=("DOUBLE", pa.float64()),
    AskPrice5=("DOUBLE", pa.float64()),
    FeedSequenceId=("INTEGER", pa.int32()),
    TS=("TIMESTAMP", pa.timestamp("ms")),
    CaptureTS=("TIMESTAMP", pa.timestamp("ms")),
)

# Data config
INSTRUMENTS = {
    7907: "FGBMU8",  # count:16559 min:103.14 max:103.34
    7935: "FGBMZ8",  # count:6489 min:103.46 max:103.665
    7988: "FGBXZ8",  # count:10917 min:113.5 max:114.29
    7993: "FGBSU8",  # count:532 min:91.2 max:91.52
    8001: "FGBSZ8",  # count:1823 min:90.6 max:91.72
    8009: "FGBXU8",  # count:21792 min:114.11 max:114.63
    8010: "FGBLU8",  # count:19557 min:108.13 max:108.595
    8028: "FGBLZ8",  # count:9423 min:108.355 max:108.81
}
RINSTRUMENTS = dict((v, k) for k, v in INSTRUMENTS.items())


def fix_trandatetime(source_dir, target_dir, csv_file):
    csv_in_path = os.path.join(source_dir, csv_file)
    csv_base, csv_ext = os.path.splitext(csv_file)
    csv_out_name = f"{csv_base}_pd{csv_ext}"
    csv_base = f"{csv_base}_pd"
    csv_out_path = os.path.join(target_dir, csv_out_name)
    df = pd.read_csv(
        csv_in_path,
        usecols=nd_consts.EXF_COLUMNS.keys(),
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


# currently unused: we may revert to using Duck CSV processing in future
def write_parquet_duck(base_name, csv_in_path, target_dir):
    colnames = list(nd_consts.EXF_COLUMNS.keys())
    pq_out_path = os.path.join(target_dir, f"{base_name}.parquet")
    read_csv_args = dict(
        header=True,
        columns=nd_consts.EXF_COLUMNS,
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
    colnames = list(nd_consts.EXF_COLUMNS.keys())
    coltypes =  map(lambda x: x[1], nd_consts.EXF_COLUMNS.values())
    for sf in source_files:
        source_path = os.path.join(nd_consts.PQ_DIR, sf)
        base_name, csv_out_path = fix_trandatetime(
            nd_consts.PQ_DIR, nd_consts.PQ_DIR, sf
        )
        pq_out_path = nd_utils.write_parquet_arrow(base_name, csv_out_path,
                                        nd_consts.PQ_DIR, colnames, coltypes)
        logr.info(f"{pq_out_path} written")
