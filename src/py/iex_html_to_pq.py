# builtin
import xml.sax
from io import StringIO
import csv
import os.path
# pyarrow
import pyarrow as pa
import pyarrow.csv as pa_csv
import pyarrow.parquet as pq
# nodom
import nd_utils
import nd_consts

IEX_COLUMNS = {
    "ticker":pa.string(),
    "date":pa.date32(),
    "name":pa.string(),
}

class IEXSymbolHandler(xml.sax.ContentHandler):
    def __init__(self):
        super().__init__()
        self.current_instrument = None
        self.instruments = []
        self.current_field = 0
        self.field_names = ['ticker','date','name']

    def startElement(self, name, attrs):
        if name == 'tr':    # new instrument tow
            self.current_instrument = dict()
            self.current_field = -1
        elif name == 'td':
            self.current_field += 1

    def characters(self, content):
        # SAX can split a single chunk of text into multiple calls,
        # so always buffer the characters continuously.
        field_name = self.field_names[self.current_field]
        if field_name in self.current_instrument:
            self.current_instrument[field_name] += content
        else:
            self.current_instrument[field_name] = content

    def endElement(self, name):
        if name == "tr":
            self.instruments.append(self.current_instrument)
            self.current_instrument = None


if __name__ == "__main__":
    base_name = "iex_instruments"
    html_in_path = os.path.join(nd_consts.PQ_DIR, f"{base_name}.html")
    with open(html_in_path, "rt") as html_file:
        iex_html = html_file.read()
        handler = IEXSymbolHandler()
        xml.sax.parseString(iex_html, handler)
        for inst in handler.instruments:
            print(inst)
        csv_out_path = os.path.join(nd_consts.PQ_DIR, f"{base_name}.csv")
        with open(csv_out_path, "w", newline='') as csv_file:
            dict_writer = csv.DictWriter(csv_file, handler.field_names, dialect='excel')
            dict_writer.writeheader()
            dict_writer.writerows(handler.instruments)
            pq_out_path = nd_utils.write_parquet_arrow(base_name,
                csv_out_path, nd_consts.PQ_DIR, handler.field_names, IEX_COLUMNS.values())
