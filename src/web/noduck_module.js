// noduck: Duck-less main handler thread implementation

// let our own event handler know window.__nodom__.duck_db is available
// tried document.postMessage(), window.postMessage and self.postMessage
// non get thru to main.ts, so we have to stick with the inefficient
// polling of __nodom__ on each render...
// 2025-11-26: we've dropped TypeScript, but still want to minimize
// dependence on browser JS msg infra
window.postMessage({ nd_type: "Online", quote_id: "NoDuck" });

// JSON serialization monkey poatch for BigInt supplied in DuckDB results
BigInt.prototype.toJSON = function () {
  return this.toString(10);
};

const nd_null = "null";

// var on_db_result to allow redefinition if emscripten Module is defined
var on_db_result = function (result_object) {
  console.log("on_db_result_js: " + JSON.stringify(result_object, null, 2));
};

self.onmessage = async (event) => {
  const nd_db_request = event.data;
  switch (nd_db_request.nd_type) {
    case "Command":
      on_db_result({
        nd_type: "CommandResponse",
        query_id: nd_db_request.query_id,
        error: "noduck",
      });
      break;
    case "Query":
      on_db_result({
        nd_type: "QueryResult",
        query_id: nd_db_request.query_id,
        error: "noduck",
      });
      break;
    case "BatchRequest":
      on_db_result({
        nd_type: "BatchResponse",
        query_id: nd_db_request.query_id,
        error: "noduck",
      });
      break;
    case "QueryResult":
    case "CommandResult":
    case "BatchResponse":
    case "FunctionResult":
      // we do not process our own results!
      break;
    case "Online":
      on_db_result({ nd_type: "Online", query_id: "NoDuck" });
      break;
    case "FunctionAsync":
      try {
        await Module["nodom_functions"][nd_db_request.query_id](nd_db_request);
      }
      catch (err) {
        console.error(err.message);
      }
      break;
    default:
      console.error(
        "noduck_module.onmessage: unexpected request: " + nd_db_request.nd_type,
        event,
      );
  }
};

self.onbeforeunload = (event) => {
  console.log("onbeforeunload\n");
  event.preventDefault();
  // event.returnValue = true;
};
