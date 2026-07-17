function FInc1(data, rv) {
  var old_val = data["op1"];
  data["op1"] += 1;
  rv.cache_key = "op1";
  rv.new_value = data["op1"];
  rv.old_value = old_val;
}

function FInc2(data, rv) {
  var old_val = data["op1"];
  data["op1"] += 2;
  rv.cache_key = "op1";
  rv.new_value = data["op1"];
  rv.old_value = old_val;
}

async function FDec1(nd_request) {
  var url = "https://localhost/api/fdec1/" + nd_request.data["op1"];
  console.log("FDec1: awaiting " + url);
  var resp = await fetch(url);
  if (resp.ok) {
    let new_value = await resp.json();
    let old_value = nd_request.data["op1"];
    nd_request.data["op1"] = new_value;
    let rv = {
      nd_type: "FunctionResult",
      query_id: nd_request.query_id,
      cache_key: "op1",
      new_value: new_value,
      old_value: old_value
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
    let old_value = nd_request.data["op1"];
    nd_request.data["op1"] = new_value;
    let rv = {
      nd_type: "FunctionResult",
      query_id: nd_request.query_id,
      cache_key: "op1",
      new_value: new_value,
      old_value: old_value
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
