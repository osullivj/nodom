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
        // DuckDB config: we take TS as dbl not BigInt
        // BigInt holding TS micro eg 1,220,227,200,000===2008-09-01 TS
        // and needs 47 bits: 011C 1B35 7400. So will fit in JS number.
        // castTimestampToDate
        //     true:Date64<millis>
        //     false:Timestamp<microsecond>
        castTimestampToDate: false,
        castBigIntToDouble: true, 
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

const nd_null = "null";

// var on_db_result to allow redefinition if emscripten Module is defined
var on_db_result = function(result_object) {
    console.log("on_db_result: " + JSON.stringify(result_object, null, 2));
};

if (typeof Module !== 'undefined') {
    let on_db_result_cpp = Module.cwrap('on_db_result_cpp', 'void', ['object']);
    let get_chunk_cpp = Module.cwrap('get_chunk_cpp', 'number', ['string', 'number']);
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

// DuckDB-WASM uses Apache Arrow JS as the result set API
// Which is NOT isomorphic with the DuckDB C API as there
// is no [u]int8, [u]int16, [u]int32, [u]int64 etc.
// https://github.com/apache/arrow-js/blob/main/src/enum.ts
// Here we decide how many WASM bytes are used to store
// the arrow-js type
function get_duck_type_size(tipe) {
    switch (tipe) {
        case 3:         // Float
        case 5:         // Utf8
        case 8:         // Date: int32_t days or int64_t milliseconds since the UNIX epoch
        case 10:        // Timestamp: Exact timestamp encoded with int64 since UNIX epoch (Default unit millisecond)
            return 8;   // 8 bytes wide
        case 2:         // Int: Signed or unsigned 8, 16, 32, or 64-bit little-endian integer
            return 4;   // 4 bytes wide
    };
    return 0;
}


function get_value(x, tipe) {

    if (typeof x?.valueOf === 'function') {
        return x.valueOf();
    }
    return x;
}


function batch_materializer(qid, batch) {
    // First, extract metadata from batch...
    let row_count = batch.numRows;
    let types = batch.schema.fields.map((d) => d.type.typeId);
    let type_objs = batch.schema.fields.map((d) => d.type);
    let names = batch.schema.fields.map((d) => d.name);
    console.log('batch_materializer: types='+types);
    console.log('batch_materializer: typ_o='+type_objs);
    console.log('batch_materializer: names='+names+'\n');
    // ...second, calc batch size in mem
    // Usually 2048 rows per chunk. Assume each col is 64bits wide...
    // row_count * types.length * 8 (bytes per word)
    // types.length for type info
    // sum(strlen+1) for names
    // Assuming 32bit words...
    let names_sum_length = names.reduce((accumulator, current_value) => {
        return accumulator + current_value.length + 1;
    }, 0);
    // space for columns, assuming 64bit wide
    let buffer_size = row_count * types.length * 2; // 32bit word *2 = 64bits
    // 32bits for each type, and 32bit chars for col names,
    // and 32bits for column count
    buffer_size += (types.length + names_sum_length) + 3; // 3 for done, cols, row_count
    // Get C++ wasm to create buffer. NB cwrapped funcs not
    // recognised inside the generator, so we Module.ccall
    let buffer_offset = Module.ccall('get_chunk_cpp', 'number', ['string', 'number'], [qid, buffer_size]);
    // Now create Uint64Array with underlying storage buffer_offset, which is a uint64_t ptr,
    // so guaranteed to be on an 8 byte boundary. So we know that will work for
    // 32,16 and 8 arrays. Note the element count adjustment in buffer_size/2.
    // Obviously half as many array elements in same size for 64 vs 32.
    let dbl_heap64 = new Float64Array(Module.HEAPU64.buffer, buffer_offset, buffer_size/2);
    let bui_heap64 = new BigUint64Array(Module.HEAPU64.buffer, buffer_offset, buffer_size/2);
    let heap32 = new Uint32Array(Module.HEAPU32.buffer, buffer_offset, buffer_size);
    // Now write the first block; metadata done, number of cols, then row count
    let bptr = 0;
    heap32[bptr++] = batch.done;
    heap32[bptr++] = types.length;
    heap32[bptr++] = row_count;
    // Now column types, addresses and names
    types.forEach((tipe) => {heap32[bptr++] = tipe;});
    // Memoize the start of the col addresses as 0 until we
    // can calc caddrs below...
    let caddr = bptr;
    types.forEach((tipe) => {heap32[bptr++] = 0;});
    names.forEach((name) => {
        for (var i = 0; i < name.length; i++) {
            heap32[bptr++] = name.charCodeAt(i);
        }
        // null terminator for string
        heap32[bptr++] = 0;
    });
    // Now write a block for each column, starting on 8 byte boundary
    // bptr has 32bit/4byte stride, so if it's odd it's not on 8 byte boundary
    if (bptr % 2) bptr++;
    // Now write the columns into the wasm chunk...
    for (var ic = 0; ic < types.length; ic++) {
        let vec = batch.getChildAt(ic);
        let tipe = types[ic]
        let sz = get_duck_type_size(tipe);
        console.log('batch_materializer: ic='+ic+', bptr='+bptr+', tipe='+tipe+', sz='+sz+'\n');
        // record the addr of the column
        heap32[caddr+ic] = bptr;
         // tipe(32) and count(32) as sanity checks at head of col
        heap32[bptr++] = tipe;
        heap32[bptr++] = sz;
        // Use heap32 or heap64 depending on column size
        if (tipe == 5) {    // string
            // varchar: variable length. We'll take 8 or less bytes,
            // and store them via stringToUTF8
            let bptr8 = buffer_offset + (bptr * 4);
            for (var ir = 0; ir < row_count; ir++) {
                // JSON.stringify handles vec.get(ir) returning
                // null gracefully, unlike toString()
                let sval = JSON.stringify(vec.get(ir));
                // strip away quotes
                sval = sval.replaceAll('"','');
                // we'll use 8 bytes per str to show a max
                // of 7 chars to keep a regular stride,
                stringToUTF8(sval, bptr8, 8);
                bptr8 += 8;
            }
            bptr += row_count * 2;
        }
        else if (sz == 4) {          // continue with heap32
            for (var ir = 0; ir < row_count; ir++) {
                heap32[bptr+ir] = vec.get(ir);
            }
            bptr += row_count;
        }
        else if (sz == 8) {     // switch to heap64
            // tipe:3|8|10,dbl,date,ts
            // At this point we know bptr is on an 8 byte boundary
            // because col block starts on 8 boundary, and we've
            // just written 8 above, so bptr/2 gives us the heap64
            // addr...
            let bptr64 = bptr/2;
            for (var ir = 0; ir < row_count; ir++) {
                switch (tipe) {
                case 3:     // Float
                    dbl_heap64[bptr64+ir] = get_value(vec.get(ir), tipe);
                    break;
                case 8:     // Date
                    break;
                case 10:    // Timestamp stored as dbl
                    dbl_heap64[bptr64+ir] = get_value(vec.get(ir), tipe);                
                    break;
                }
            }
            bptr += row_count * 2;
        }
        else {
            let val = get_value(vec.get(0), tipe);
            console.error('batch_materializer: bad sz!\n');
        }
    }
    // Let C++ WASM code know we've populated the chunk
    // We pass buffer_offset as number/int, and cast to uint8_t on
    // the C++ side
    Module.ccall('on_chunk_cpp', 'void', ['number'], [buffer_offset]);
    console.log('batch_materializer: buffer_offset='+buffer_offset+', row_count='+row_count+'\n');
    if (batch.done) return 0;
    return buffer_offset;
}

async function* batch_generator(query_id, duck_result) {
    for await (const batch of duck_result) {
        yield batch_materializer(query_id, batch);  
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
            batch_gen = batch_generator(nd_db_request.query_id, duck_result);
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
                                    chunk:batch_addr/1};
                console.log("duck_module: BatchResponse: " + batch_result + "\n");                                    
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