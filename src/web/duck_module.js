// hduck: DuckDB handler thread implementation
// Main browser thread: running imgui
// DB thread: DuckDB wasm engine thread
// This thread
//   Submit queries to DDBW
//   Pre process query results
//   Fetch Parquet and load into DDBW

// Some worker scope vars...
let duck_db = null;
let duck_conn = null;

// This JS impl was in index.html: by doing it here we avoid having to pass the
// DB handle across threads. NB this module must be pulled in to index.html
// like so...
// <script type="module" src="./duck_module.js" defer></script>

// This import was "import * as duck" so we could scope the duck names.
// However, it looks like the duck shell stuff needs these names to be
// at the top level
import * as duck from "https://cdn.jsdelivr.net/npm/@duckdb/duckdb-wasm@latest/+esm";
import * as shell from "https://cdn.jsdelivr.net/npm/@duckdb/duckdb-wasm-shell@latest/+esm";
// Cannot import wasm; we have to fetch
const shell_wasm = await fetch("https://cdn.jsdelivr.net/npm/@duckdb/duckdb-wasm-shell/dist/shell_bg.wasm");

const JSDELIVR_BUNDLES = duck.getJsDelivrBundles();
const bundle = await duck.selectBundle(JSDELIVR_BUNDLES);
// creates storage and an address for the DB engine worker thread
const db_worker_url = URL.createObjectURL(
    new Blob([`importScripts("${bundle.mainWorker}");`], {type: "text/javascript",}));
const db_worker = new Worker(db_worker_url);
const logger = new duck.ConsoleLogger();
duck_db = new duck.AsyncDuckDB(logger, db_worker);
// loads the web assembly module into memory and configures it
await duck_db.instantiate(bundle.mainModule, bundle.pthreadWorker);
// revoke the object url now no longer needed
URL.revokeObjectURL(db_worker_url);
await duck_db.open({
    query: { 
        // castBigIntToDouble: true, 
        castDecimalToDouble: true
    },
});
console.log("duck_module.js: DuckDB instantiated ", db_worker_url);
// main.ts uses __nodom__
window.__nodom__ = {duck_module:self, duck_db:duck_db};
// let our own event handler know window.__nodom__.duck_db is available
// tried document.postMessage(), window.postMessage and self.postMessage
// non get thru to main.ts, so we have to stick with the inefficient
// polling of __nodom__ on each render...
// 2025-11-26: we've dropped TypeScript, but still want to minimize
// dependence on browser JS msg infra
window.postMessage({nd_type:"DuckInstance"});

// JSON serialization monkey poatch for BigInt supplied in DuckDB results
BigInt.prototype.toJSON = function() {return this.toString(10);};

// var on_db_result to allow redefinition if emscripten Module is defined
var on_db_result = function(result_object) {
    console.log("on_db_result: " + JSON.stringify(result_object, null, 2));
};

if (typeof Module !== 'undefined') {
    let on_db_result_cpp = Module.cwrap('on_db_result_cpp', 'void', ['object']);
    let get_chunk_cpp = Module.cwrap('get_chunk_cpp', 'number', ['number']);
    let on_chunk_cpp = Module.cwrap('on_chunk_cpp', 'void', ['number']);
    on_db_result = function(result_object) {
        let result_handle = Emval.toHandle(result_object);
        on_db_result_cpp(result_handle);
    };
}

async function exec_duck_db_query(sql) {
    if (!duck_db) {
        console.error("duck_module:DuckDB-Wasm not initialized");
        return;
    }
    if (!duck_conn) {
        console.log("duck_module:reconnecting...");
        duck_conn = await duck_db.connect();
    }
    console.log("exec_duck_dq_query: " + sql);
    let duck_result = await duck_conn.send(sql);
    return duck_result;
}


function batch_materializer(batch) {
    let row_count = batch.numRows;
    let types = batch.schema.fields.map((d) => d.type);
    let names = batch.schema.fields.map((d) => d.name);
    // Usually 2048 rows per chunk. Assume each col is 64bits wide...
    // row_count * types.length * 8 (bytes per word)
    // types.length for type info
    // sum(strlen+1) for names
    // Assuming 16bit words...
    let names_sum_length = names.reduce((accumulator, current_value) => {
        return accumulator + current_value.length + 1;
    }, 0);
    // space for columns, assuming 64bit wide
    let buffer_size = row_count * types.length * 4;
    // 16bits for each type, and 16bit chars for col names,
    // and 16bits for column count
    buffer_size += (types.length) + names_sum_length + 3;
    // Get C++ wassm to create buffer. NB cwrapped funcs not
    // recognised inside the generator, so we Module.ccall
    let buffer_offset = Module.ccall('get_chunk_cpp', 'number', ['number'], [buffer_size]);
    // Now create Uint8Array with underlying storage buffer_offset, which is a ptr,
    // and therefore just an index into Module.HEAPU8 WASM memory
    let heap_view16 = new Uint16Array(Module.HEAPU16.buffer, buffer_offset, buffer_size);
    // load the data: first number of cols, then rows
    let bptr16 = 0;
    heap_view16[bptr16++] = batch.done;
    heap_view16[bptr16++] = types.length;
    heap_view16[bptr16++] = row_count;
    types.forEach((tipe) => {heap_view16[bptr16++] = tipe;});
    names.forEach((name) => {
        for (var i = 0; i < name.length; i++) {
            // 3rd parm is true for little endian as wasm is little endian
            heap_view16[bptr16++] = name.charCodeAt(i);
        }
        // null terminator for string
        heap_view16[bptr16++] = 0;
    });
    // Let C++ WASM code know we've populated the chunk
    // We pass buffer_offset as number/int, and cast to uint8_t on
    // the C++ side
    Module.ccall('on_chunk_cpp', 'void', ['number'], [buffer_offset]);
    console.log('batch_materializer: buffer_offset='+buffer_offset+', row_count='+row_count+'\n');
    if (batch.done) return 0;
    return buffer_offset;
}

async function* batch_generator(duck_result) {
    for await (const batch of duck_result) {
        yield batch_materializer(batch);  
    }
}

let global_query_map = new Map();

self.onmessage = async (event) => {
    let duck_result = null;
    let batch_gen = null;
    const nd_db_request = event.data;
    switch (nd_db_request.nd_type) {
        case "ParquetScan":
            // NB result set from "CREATE TABLE <tbl> as select * from parquet_scan([...])"
            // is None on success
            await exec_duck_db_query(nd_db_request.sql);
            console.log("duck_module: ParquetScan done for " + nd_db_request.query_id);
            on_db_result({nd_type:"ParquetScanResult",query_id:nd_db_request.query_id});
            break;
        case "Query":
            duck_result = await exec_duck_db_query(nd_db_request.sql);
            console.log("duck_module: QueryResult: " + nd_db_request.query_id + "\n");
            batch_gen = batch_generator(duck_result);
            global_query_map.set(nd_db_request.query_id, batch_gen);
            let query_result = {nd_type:"QueryResult",query_id:nd_db_request.query_id};
            on_db_result(query_result);
            break;
        case "BatchRequest":
            if (global_query_map.has(nd_db_request.query_id)) {
                console.log("duck_module: BatchRequest: " + nd_db_request.query_id + "\n");
                batch_gen = global_query_map.get(nd_db_request.query_id);
                let batch_addr = await batch_gen.next();
                let batch_result = {nd_type:"BatchResponse",
                                    query_id:nd_db_request.query_id,
                                    chunk:(batch_addr/1)};
                on_db_result(batch_result);
            }
            else {
                on_db_result({nd_type:"BatchResponse",query_id:nd_db_request.query_id, error:"unknown"});
            }
            break;
        case "QueryResult":
        case "ParquetScanResult":
        case "BatchResponse":
            // we do not process our own results!
            break;
        case "DuckInstance":
            on_db_result({nd_type:"DuckInstance", query_id:"TEST_QUERY_0"});
            break;
        default:
            console.error("duck_module.onmessage: unexpected request: ", event);
    }
};