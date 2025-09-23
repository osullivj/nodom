# builtin
import logging
import os
import os.path
# 3rd pty
import pandas as pd
import pyarrow as pa
import pyarrow.csv as pa_csv
import pyarrow.parquet as pq
# h3gui
import nd_consts
import nd_utils

logr = nd_utils.init_logging('pq_writer', console=True)

def write_parquet(base_name, csv_in_path, target_dir):
    colnames = list(nd_consts.COLUMNS.keys())
    # skip_rows=1 to ignore col headers as we specify the col names
    # and types, and have autogenerate_column_names=False
    read_options = pa_csv.ReadOptions(column_names=colnames, autogenerate_column_names=False, skip_rows=1)
    convert_options = pa_csv.ConvertOptions(include_columns=colnames,
                                            column_types=nd_consts.COLUMNS,
                                     include_missing_columns=False,
                                     strings_can_be_null=False)
    table = pa_csv.read_csv(csv_in_path, read_options=read_options, convert_options=convert_options)
    pq_out_path = os.path.join(target_dir, f'{base_name}.parquet')
    logr.info(f'== writing {pq_out_path}')
    logr.info(f'columns:{table.column_names}')
    logr.info(f'schema:{table.schema}')
    pq.write_table(table, pq_out_path)
    metadata = pq.read_metadata(pq_out_path)
    logr.info(f'metadata:{metadata}')
    return pq_out_path


if __name__ == '__main__':
    nd_utils.init_logging(__file__)
    source_files = nd_utils.file_list(nd_consts.PQ_DIR, 'FGB??8_200809[01]?.csv')
    logr.info(f'Source CSVs found in {nd_consts.PQ_DIR}\n{source_files}')
    for sf in source_files:
        source_path = os.path.join(nd_consts.PQ_DIR, sf)
        base_name = os.path.splitext(sf)[0]
        pq_out_path = write_parquet(base_name, source_path, nd_consts.PQ_DIR)
        logr.info(f'{pq_out_path} written')
