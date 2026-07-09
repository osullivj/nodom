function FInc1(data) {
  data["op1"] += 1;
  return {
    nd_type: "DataChange",
    cache_key: "op1",
    new_value: data["op1"],
  };
}

function FInc2(data) {
  data["op1"] += 2;
  return {
    nd_type: "DataChange",
    cache_key: "op1",
    new_value: data["op1"],
  };
}

window["__nodom__"] = {
  functions: [FInc1, FInc2],
};
