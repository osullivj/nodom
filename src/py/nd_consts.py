import os.path
import pyarrow as pa
import sys

# B64_RSA_KEY was extracted from aioquic/tests/ssl_cert.pem
B64_RSA_KEY = 'BSQJ0jkQ7wwhR7KvPZ+DSNk2XTZ/MS6xCbo9qu++VdQ='

# Env config
if sys.platform == 'linux':
    CHROME_EXE = r'/opt/google/chrome/chrome'
else:
    CHROME_EXE = r'C:\Program Files (x86)\Google\Chrome\Application\chrome.exe'

# Notes on Chrome launch flags...
# https://peter.sh/experiments/chromium-command-line-switches/
# https://www.chromium.org/developers/how-tos/run-chromium-with-flags/
CHROME_LAUNCH_FMT = (
    '%(exe)s --user-data-dir=%(user_data_dir)s --no-proxy-server '
    # logging into user_data_dir: comment out to restore logging in devtools
    # '--enable-logging --v=1 '
    '--auto-open-devtools-for-tabs '    # to run debugger
    '--bwsi '                           # no sign in
    # we need both these to get Chrome to accept a self signed cert from the server
    '--ignore-cerificate-errors '
    '--ignore-certificate-errors-spki-list=%(b64_rsa_key)s '
    '--disable-extensions '
    # https://medium.com/@aleksej.gudkov/understanding-and-fixing-the-strict-origin-when-cross-origin-cors-error-340c6614f701
    #   '--disable-web-security '
    # Python Tornado based servers on 8890, npm on 8080
    'http://localhost:8890/index.html'
)

CHROME_LAUNCH_DICT = dict(exe=CHROME_EXE, user_data_dir='', b64_rsa_key=B64_RSA_KEY)

ND_ROOT_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..'))
PQ_DIR = os.path.normpath(os.path.join(ND_ROOT_DIR, 'dat'))

# Data config
INSTRUMENTS = {
    7907: 'FGBMU8', # count:16559 min:103.14 max:103.34
    7935: 'FGBMZ8', # count:6489 min:103.46 max:103.665
    7988: 'FGBXZ8', # count:10917 min:113.5 max:114.29
    7993: 'FGBSU8', # count:532 min:91.2 max:91.52
    8001: 'FGBSZ8', # count:1823 min:90.6 max:91.72
    8009: 'FGBXU8', # count:21792 min:114.11 max:114.63
    8010: 'FGBLU8', # count:19557 min:108.13 max:108.595
    8028: 'FGBLZ8', # count:9423 min:108.355 max:108.81
}
RINSTRUMENTS=dict((v,k) for k,v in INSTRUMENTS.items())

COLUMNS=dict(
    SeqNo=pa.int32(),
    LastTradeTime=pa.timestamp('s'),
    LastTradePrice=pa.float64(),
    LastTradeSize=pa.int32(),
    HighPrice=pa.float64(),
    LowPrice=pa.float64(),
    Volume=pa.int32(),
    LastTradeSequence=pa.int32(),
    TranDateTime=pa.timestamp('ms'),
    BidQty1=pa.int32(),
    AskQty1=pa.int32(),
    BidQty2=pa.int32(),
    AskQty2=pa.int32(),
    BidQty3=pa.int32(),
    AskQty3=pa.int32(),
    BidQty4=pa.int32(),
    AskQty4=pa.int32(),
    BidQty5=pa.int32(),
    AskQty5=pa.int32(),
    BidPrice1=pa.float64(),
    AskPrice1=pa.float64(),
    BidPrice2=pa.float64(),
    AskPrice2=pa.float64(),
    BidPrice3=pa.float64(),
    AskPrice3=pa.float64(),
    BidPrice4=pa.float64(),
    AskPrice4=pa.float64(),
    BidPrice5=pa.float64(),
    AskPrice5=pa.float64(),
    FeedSequenceId=pa.int32(),
    TS=pa.timestamp('ms'),
    CaptureTS=pa.timestamp('ms'),
)

TIME_FMT='%H:%M:%S.%f'
DATETIME_FMT='%Y-%m-%d %H:%M:%S.%f'
RAW_DATE_FORMATS = dict(time=TIME_FMT, FeedCaptureTS=TIME_FMT, LastTradeTime=DATETIME_FMT, TranDateTime=DATETIME_FMT)
CLEAN_DATE_FORMATS = dict(time=DATETIME_FMT, FeedCaptureTS=DATETIME_FMT, LastTradeTime=DATETIME_FMT, TranDateTime=DATETIME_FMT)

# see main.ts:var empty_table:Materialized
# in layout defns we need something the same "shape"
EMPTY_TABLE=dict(names=dict(length=0), rows=dict(length=0), types=dict(length=0))
