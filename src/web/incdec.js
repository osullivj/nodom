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

async function FDec1(nd_request) {
  var url = "https://localhost/api/fdec1/" + nd_request.data["op1"];
  console.log("FDec1: awaiting " + url);
  var resp = await fetch(url);
  if (resp.ok) {
    let new_value = await resp.json();
    nd_request.data["op1"] = new_value;
    let rv = {
      nd_type: "FunctionResult",
      query_id: nd_request.query_id,
      cache_key: "op1",
      new_value: new_value
    };
    const srv = JSON.stringify(rv);
    console.log("FDec1: result " + srv);
    on_async_done(srv);
    return;
  }
  let rv = {
    nd_type: "FunctionResult",
    query_id: nd_request.query_id,
    error: resp.statusText
  };
  const srv = JSON.stringify(rv);
  console.log("FDec1: result " + srv);
  on_async_done(srv);
  return;
}

async function FDec2(nd_request) {
  var url = "https://localhost/api/fdec2/" + nd_request.data["op1"];
  console.log("FDec2: awaiting " + url);
  var resp = await fetch(url);
  if (resp.ok) {
    let new_value = await resp.json();
    nd_request.data["op1"] = new_value;
    let rv = {
      nd_type: "FunctionResult",
      query_id: nd_request.query_id,
      cache_key: "op1",
      new_value: new_value
    };
    const srv = JSON.stringify(rv);
    console.log("FDec2: result " + srv);
    on_async_done(srv);
    return;
  }
  let rv = {
    nd_type: "FunctionResult",
    query_id: nd_request.query_id,
    error: resp.statusText
  };
  const srv = JSON.stringify(rv);
  console.log("FDec2: result " + srv);
  on_async_done(srv);
  return;
}

Module["nodom_functions"] = [FInc1, FInc2, FDec1, FDec2];
