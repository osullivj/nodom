Module["onRuntimeInitialized"] = function () {
  if (typeof Module !== "undefined") {
    let on_db_result_cpp = Module.cwrap("on_db_result_cpp", "void", ["object"]);
    let get_chunk_cpp = Module.cwrap("get_chunk_cpp", "number", [
      "string",
      "number",
    ]);
    let on_chunk_cpp = Module.cwrap("on_chunk_cpp", "void", ["number"]);
    on_db_result = function (result_object) {
      let result_handle = Emval.toHandle(result_object);
      on_db_result_cpp(result_handle);
    };
    on_async_done = function (srv) {
      const psrv = Module.stringToNewUTF8(srv);
      try {
        Module._on_async_done(psrv);
      }
      finally {
        Module._free(psrv);
      }
      return;
    };
  }
};
