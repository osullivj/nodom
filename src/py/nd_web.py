# std pkgs
from datetime import datetime
import json
import os.path
import uuid
# tornado
import tornado
import tornado.websocket
from tornado.options import define, options
from tornado.web import StaticFileHandler
# nodom
import nd_utils
import nd_consts

# command line option definitions: same for all tornado procs
# individual server impls will set "port"
define("debug", default=True, help="run in debug mode")
define( "host", default="localhost")
define( "node_port", default=8080)
# tornado logs to stderr by default; however we have nd_utils.init_logging,
# which is unaware of the tornado log setup. We cannot set here as Tornado's
# log.py defines, so we set on the cmd line --log-to-stderr

logr = nd_utils.init_logging(__name__)

GOOD_HTTP_ORIGINS = ['https://shell.duckdb.org', 'https://sql-workbench.com']

CHUNK_SIZE = 2**16  # 64K chunks
BUFFER = bytearray(CHUNK_SIZE)
parquet_path = lambda pq_name: os.path.join(nd_consts.ND_ROOT_DIR, 'dat', pq_name)
test_data_path = lambda test_name, slug: os.path.join(nd_consts.ND_ROOT_DIR, 'dat', 'test', test_name, f'{slug}.json')

logr = nd_utils.init_logging(__name__, True)

# Tornado implements HTTP ranges in StaticFileHandler
# However, DuckDB-Wasm can only invoke HTTPS from inside
# wasm code because of sandboxing. We also have to supply
# CORS headers that allow the Parquet server to have a
# diff URL from the GUI server. Hence a subclass...
class ParquetHandler(tornado.web.StaticFileHandler):
    def set_default_headers(self, *args, **kwargs):
        # https://www.marginalia.nu/log/a_105_duckdb_parquet/
        # https://developer.mozilla.org/en-US/docs/Web/HTTP/Range_requests
        # https://github.com/mozilla/pdf.js/issues/8566
        self.set_header("Access-Control-Allow-Origin", "*") # allow access by shell.duckdb.org
        self.set_header("Access-Control-Allow-Headers", "Range, Origin, X-Requested-With, Content-Type, Accept, Authorization")
        self.set_header("Access-Control-Expose-Headers","Content-Length, Content-Encoding, Accept-Ranges, Content-Range")
        self.set_header('Access-Control-Allow-Methods', ' GET, HEAD, OPTIONS')
        # TODO: note cacheEpoch in DuckDBs browser_runtime.ts
        # Does is come from this header? We'll need to think about cache
        # eviction strategies...
        self.set_header('Access-Control-Max-Age', '86400')
        # accept Ranged requests for chunks
        self.set_header("Accept-Ranges", "bytes")
        self.set_header("Connection", "keep-alive")


class HomeHandler(tornado.web.RequestHandler):
    def set_default_headers(self, *args, **kwargs):
        self.set_header("Content-Type", "text/html")
        self.set_header("Access-Control-Allow-Origin", f"*")

    def get(self):
        self.render("index.html", duck_db=self.application.service.is_duck_app)


class APIHandlerBase(tornado.web.RequestHandler):
    def set_default_headers(self, *args, **kwargs):
        self.set_header("Content-Type", "application/json")
        self.set_header("Access-Control-Allow-Origin", f"http://{options.host}:{options.node_port}")

class DuckJournalHandler(tornado.web.RequestHandler):
    def get(self, slug):
        self.set_header("Content-Type", "text/plain")
        response_text = self.application.service.on_duck_journal_request(slug)
        self.write(response_text)
        self.finish()

class JSONHandler(APIHandlerBase):
    def get(self, slug):
        response_json = self.application.service.on_api_request(slug)
        save_json_path = test_data_path(self.application.service.app_name, slug)
        with open(save_json_path, 'wt') as jsonf:
            jsonf.write(response_json)
        self.write(response_json)
        self.finish()


class WebSockHandler(tornado.websocket.WebSocketHandler):
    def check_origin(self, origin):
        return True

    def open(self):
        self._uuid = str(uuid.uuid4())
        logr.info(f'WebSockHandler.open: {self._uuid}')
        self.application.on_ws_open(self)

    def on_close(self):
        logr.info(f'WebSockHandler.on_close: {self._uuid}')
        self.application.on_ws_close(self)

    def on_message(self, msg):
        logr.info(f'WebSockHandler.on_message: {self._uuid} IN {msg}')
        msg_dict = json.loads(msg)
        msg_dict['uuid'] = self._uuid
        self.application.on_ws_message(self, msg_dict)






ND_HANDLERS = [
    (r'/example/index.html', HomeHandler),
    (r'/api/websock', WebSockHandler),
    (r'/api/(.*)', JSONHandler),
    (r'/ui/duckjournal/(.*)', DuckJournalHandler),
    (r'/imgui/misc/fonts/(.*)', StaticFileHandler, dict(path=os.path.join(nd_consts.ND_ROOT_DIR, 'imgui', 'misc', 'fonts'))),
    (r'/node_modules/@flyover/system/build/(.*)', StaticFileHandler, dict(path=os.path.join(nd_consts.ND_ROOT_DIR, 'node_modules', '@flyover', 'system', 'build'))),
    (r'/example/build/(.*)', StaticFileHandler, dict(path=os.path.join(nd_consts.ND_ROOT_DIR, 'example', 'build'))),
    (r'/build/(.*)', StaticFileHandler, dict(path=os.path.join(nd_consts.ND_ROOT_DIR, 'build'))),
    (r'/example/(.*)', StaticFileHandler, dict(path=os.path.join(nd_consts.ND_ROOT_DIR, 'example'))),
    (r'/src/(.*)', StaticFileHandler, dict(path=os.path.join(nd_consts.ND_ROOT_DIR, 'src'))),
    (r'/(.*)', StaticFileHandler, dict(path=nd_consts.ND_ROOT_DIR)),
]

class NDApp( tornado.web.Application):
    def __init__( self, service, extra_handlers = []):
        # extra_handlers first so they get first crack at the match
        self.service = service
        handlers = extra_handlers + ND_HANDLERS
        settings = dict(template_path=os.path.join(nd_consts.ND_ROOT_DIR, 'imgui', 'example'))
        tornado.web.Application.__init__( self, handlers, **settings)
        self.ws_handlers = service.get_ws_handlers()

    def on_ws_open(self, websock):
        logr.info(f'NDApp.on_ws_open: {websock._uuid}')
        self.service.on_ws_open(websock)

    def on_ws_close(self, websock):
        logr.info(f'NDApp.on_ws_close: {websock._uuid}')
        self.service.on_ws_close(websock)

    def on_ws_message(self, websock, mdict):
        logr.info(f'NDApp.on_ws_message: {websock._uuid} {mdict}')
        msg_dict = mdict if isinstance(mdict, dict) else dict()
        handler_func = self.ws_handlers.get(msg_dict.get('nd_type'), self.service.on_no_op)
        change_list = handler_func(websock._uuid, msg_dict)
        assert isinstance(change_list, list)
        for change in change_list:
            websock.write_message(change)
