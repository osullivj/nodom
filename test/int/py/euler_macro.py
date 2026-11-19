from tornado.options import define, options, parse_command_line
import pandas as pd
from eulerpool import Eulerpool

def fetch_macro():
    client = Eulerpool(options.key)
    series = {
        "fed_funds": client.macro.fred_observations(seriesId="FEDFUNDS"),
        "us_cpi_yoy": client.macro.fred_observations(seriesId="CPIAUCSL", units="pc1"),
        # "ecb_depo": client.macro.ecb_observations(seriesKey="FM.D.U2.EUR.4F.KR.DFR.LEV"),
    }
    # nested dict comprehension ahoy!
    df = pd.DataFrame({
        name: pd.Series( {obs["date"]: obs["value"] for obs in data["observations"]} )
            for name, data in series.items()
    })
    df.index = pd.to_datetime(df.index)
    return df


define("key", default="", help="eulerpool key", type=str)

if __name__ == "__main__":
    parse_command_line()
    df = fetch_macro()
    print(df.resample("QE").last().tail(8))
