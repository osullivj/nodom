import os.path
import sys

import pyarrow as pa

# B64_RSA_KEY was extracted from aioquic/tests/ssl_cert.pem
B64_RSA_KEY = "BSQJ0jkQ7wwhR7KvPZ+DSNk2XTZ/MS6xCbo9qu++VdQ="

# Env config
if sys.platform == "linux":
    CHROME_EXE = r"/opt/google/chrome/chrome"
else:
    CHROME_EXE = r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe"

# Notes on Chrome launch flags...
# https://peter.sh/experiments/chromium-command-line-switches/
# https://www.chromium.org/developers/how-tos/run-chromium-with-flags/
CHROME_LAUNCH_FMT = (
    "%(exe)s --user-data-dir=%(user_data_dir)s --no-proxy-server "
    # logging into user_data_dir: comment out to restore logging in devtools
    # '--enable-logging --v=1 '
    "--auto-open-devtools-for-tabs "  # to run debugger
    "--bwsi "  # no sign in
    # we need both these to get Chrome to accept a self signed cert from the server
    "--ignore-cerificate-errors "
    "--ignore-certificate-errors-spki-list=%(b64_rsa_key)s "
    "--disable-extensions "
    # https://medium.com/@aleksej.gudkov/understanding-and-fixing-the-strict-origin-when-cross-origin-cors-error-340c6614f701
    #   '--disable-web-security '
    # Python Tornado based servers on 8890, npm on 8080
    "http://localhost:8890/index.html"
)

CHROME_LAUNCH_DICT = dict(exe=CHROME_EXE, user_data_dir="", b64_rsa_key=B64_RSA_KEY)

ND_ROOT_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
PQ_DIR = os.path.normpath(os.path.join(ND_ROOT_DIR, "dat"))

TIME_FMT = "%H:%M:%S.%f"
DATETIME_FMT = "%Y-%m-%d %H:%M:%S"
TIMESTAMP_FMT = "%Y-%m-%d %H:%M:%S.%f"
DATE_FMT = "%Y-%m-%d"

RAW_DATE_FORMATS = dict(
    CaptureTS=TIMESTAMP_FMT,
    # LastTradeTime=DATETIME_FMT,
    TranDateTime=TIMESTAMP_FMT,
)
CLEAN_DATE_FORMATS = dict(
    CaptureTS=TIMESTAMP_FMT,
    LastTradeTime=TIMESTAMP_FMT,
    TranDateTime=TIMESTAMP_FMT,
)
