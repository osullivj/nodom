function FInc1(data) {
  data["op1"] += 1;
  var rv = {
    nd_type: "DataChange",
    cache_key: "op1",
    new_value: data["op1"],
  };
  console.log("FInc1: " + JSON.stringify(rv));
  return rv;
}

function FInc2(data) {
  data["op1"] += 2;
  var rv = {
    nd_type: "DataChange",
    cache_key: "op1",
    new_value: data["op1"],
  };
  console.log("FInc2: " + JSON.stringify(rv));
  return rv;
}

async function FDec1(request) {
  var url = "https://localhost/api/fdec1/" + request.data["op1"];
  console.log("FDec1: awaiting " + url);
  var resp = await fetch(url);
  console.log("FDec1: response " + resp);
  if (resp.ok) {
    data["op1"] = await resp.json();
    return {
      nd_type: "FunctionResult",
      query_id: nd_db_request.query_id,
      cache_key: "op1",
      new_value: data["op1"],
    };
    return {
      nd_type: "FunctionResult",
      query_id: nd_db_request.query_id,
      error: "fetch"
    };
  }
}

async function FDec2(request) {
  var url = "https://localhost/api/fdec2/" + request.data["op1"];
  console.log("FDec2: awaiting " + url);
  var resp = await fetch(url);
  console.log("FDec2: response " + resp);
  if (resp.ok) {
    data["op1"] = await resp.json();
    return {
      nd_type: "FunctionResult",
      query_id: nd_db_request.query_id,
      cache_key: "op1",
      new_value: data["op1"],
    };
    return {
      nd_type: "FunctionResult",
      query_id: nd_db_request.query_id,
      error: "fetch"
    };
  }
}

Module["nodom_functions"] = [FInc1, FInc2, FDec1, FDec2];
