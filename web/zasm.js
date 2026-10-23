const emFunctions = {
  doCommands: null,
  getRegister: null,
  setRegister: null,
  runtimeDebug: null,
  logError: null,
};

function assert(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

// Views of the wasm memory. Not Module.HEAPU8 and friends: when a worker thread grows the memory,
// memory.buffer becomes a new, larger SharedArrayBuffer, and those views can still be of the old
// one - shorter, so reads past its end come back undefined. (JIT codegen runs on worker threads,
// and allocates a lot.)
function heapU8() {
  return new Uint8Array(Module.wasmMemory.buffer);
}

function heapU32() {
  return new Uint32Array(Module.wasmMemory.buffer);
}

function heapI32() {
  return new Int32Array(Module.wasmMemory.buffer);
}

function readCString(ptr) {
  const u8 = heapU8();
  let end = ptr;
  while (u8[end]) end++;
  // TextDecoder can't read a SharedArrayBuffer directly.
  return new TextDecoder().decode(u8.slice(ptr, end));
}

function assertMainThread() {
  assert(typeof window !== 'undefined', 'expected to be called from main thread');
}

// Returns the engine's raw wasm export for an exported C function, so it can be
// bound as a script-module import and called wasm->wasm with no JS frame. These
// imports are on the script hot path (get/set_register per engine-register
// access, do_commands per uncompiled-op batch), and going through JS was a
// dominant cost for engine-heavy scripts. The wasm JIT only runs in the JSPI
// build, where only `main` is wrapped, so every other export is the raw function.
function rawEngineExport(name) {
  const fn = Module['_' + name];
  assert(fn, `no such engine export: ${name}`);
  return fn;
}

function ensureEmFunctions() {
  if (emFunctions.doCommands) return;

  emFunctions.doCommands = rawEngineExport('em_do_commands');
  emFunctions.directEnter = rawEngineExport('em_direct_enter');
  emFunctions.directRetstackPop = rawEngineExport('em_direct_retstack_pop');
  emFunctions.getRegister = rawEngineExport('em_get_register');
  emFunctions.setRegister = rawEngineExport('em_set_register');
  emFunctions.setGuardedRegister = rawEngineExport('em_set_guarded_register');
  emFunctions.runtimeDebug = rawEngineExport('em_runtime_script_debug');
  emFunctions.logError = rawEngineExport('em_log_error');
  emFunctions.podRead = rawEngineExport('em_pod_read');
  emFunctions.podWrite = rawEngineExport('em_pod_write');
  emFunctions.allocatemem = rawEngineExport('em_allocatemem');
  emFunctions.writepodarr = rawEngineExport('em_writepodarr');
  emFunctions.classRead = rawEngineExport('em_class_read');
  emFunctions.classWrite = rawEngineExport('em_class_write');
  emFunctions.stringCompare = rawEngineExport('em_string_compare');
  emFunctions.refChange = rawEngineExport('em_ref_change');
  emFunctions.markTypeStack = rawEngineExport('em_mark_type_stack');
  emFunctions.arraysize = rawEngineExport('em_arraysize');
}

async function compileOneModule(name, ptr, size) {
  try {
    const buffer = new Uint8Array(Module.wasmMemory.buffer, ptr, size);
    const module = await WebAssembly.compile(buffer);

    const instance = await WebAssembly.instantiate(module, {
      env: {
        memory: Module.wasmMemory,
        // For direct calls into other compiled functions (see compile_callfunc_direct).
        table: Module.wasmTable,
        do_commands: emFunctions.doCommands,
        direct_enter: emFunctions.directEnter,
        direct_retstack_pop: emFunctions.directRetstackPop,
        get_register: emFunctions.getRegister,
        set_register: emFunctions.setRegister,
        set_guarded_register: emFunctions.setGuardedRegister,
        runtime_debug: emFunctions.runtimeDebug,
        log_error: emFunctions.logError,
        pod_read: emFunctions.podRead,
        pod_write: emFunctions.podWrite,
        allocatemem: emFunctions.allocatemem,
        writepodarr: emFunctions.writepodarr,
        class_read: emFunctions.classRead,
        class_write: emFunctions.classWrite,
        string_compare: emFunctions.stringCompare,
        ref_change: emFunctions.refChange,
        mark_type_stack: emFunctions.markTypeStack,
        arraysize: emFunctions.arraysize,
      },
    });

    // The JIT driver calls `exec` through the function table (see exec_script in
    // jit_shared.cpp), with no JS frame in between. Its table slot also identifies
    // the module.
    return Module.addFunction(instance.exports.exec);
  } catch (err) {
    console.error(`error compiling ${name}`, err);
    return 0;
  }
}

// Compiles many modules concurrently - the browser compiles WebAssembly on an
// internal thread pool, so this runs in the time of the largest module rather
// than the sum. The names/ptrs/sizes arguments point at C arrays of length
// count. Resolves to the module ids (0 on failure). compileOneModule copies
// each module's bytes before its first await, so the C buffers are free to
// change once this returns.
function startCompileJobs(count, namesPtr, ptrsPtr, sizesPtr) {
  assertMainThread();
  ensureEmFunctions();

  const jobs = [];
  const u32 = heapU32();
  for (let i = 0; i < count; i++) {
    const name = readCString(u32[namesPtr / 4 + i]);
    const ptr = u32[ptrsPtr / 4 + i];
    const size = u32[sizesPtr / 4 + i];
    jobs.push(compileOneModule(name, ptr, size));
  }
  return Promise.all(jobs);
}

function writeIds(ids, outIdsPtr) {
  const i32 = heapI32();
  for (let i = 0; i < ids.length; i++) {
    i32[outIdsPtr / 4 + i] = ids[i];
  }
}

/** @type {Map<number, {ids: number[] | null, cancelled: boolean}>} */
const pendingBatches = new Map();
let nextBatchId = 1;

// Starts compiling a batch of modules (see jit_backend_prepare_commits), and
// returns its id for pollCompileScriptWasmModuleBatch.
export function startCompileScriptWasmModuleBatch(count, namesPtr, ptrsPtr, sizesPtr) {
  const id = nextBatchId++;
  const batch = { ids: null, cancelled: false };
  pendingBatches.set(id, batch);
  startCompileJobs(count, namesPtr, ptrsPtr, sizesPtr).then(ids => {
    if (batch.cancelled) {
      for (const runFn of ids) if (runFn) destroyScriptWasmModule(runFn);
      return;
    }
    batch.ids = ids;
  });
  return id;
}

// If the batch has finished, writes its module ids to outIds and returns true.
export function pollCompileScriptWasmModuleBatch(id, outIdsPtr) {
  const batch = pendingBatches.get(id);
  if (!batch?.ids) return false;

  pendingBatches.delete(id);
  writeIds(batch.ids, outIdsPtr);
  return true;
}

// Discards a batch; its modules are released once they finish compiling.
export function cancelCompileScriptWasmModuleBatch(id) {
  const batch = pendingBatches.get(id);
  if (!batch) return;

  pendingBatches.delete(id);
  batch.cancelled = true;
  if (batch.ids) {
    for (const runFn of batch.ids) if (runFn) destroyScriptWasmModule(runFn);
  }
}

// Called (via em_destroy_wasm_module) when compiled scripts are released, e.g.
// loading a different quest in the same session.
export function destroyScriptWasmModule(runFn) {
  assertMainThread();

  Module.removeFunction(runFn);
  return true;
}
