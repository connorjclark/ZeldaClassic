# JIT

`zplayer` typically executes a quest's ZASM scripts by running them in our ZASM interpreter. To speed things up significantly, we can compile ZASM scripts to native machine code using the JIT (just-in-time) compiler. This runs in `zplayer`, not in the `zscript` compiler. There are three backends, all under `src/zc/scripting/jit/`: `jit_x64.cpp` (x86-64), `jit_a64.cpp` (AArch64, e.g. Apple Silicon), and `jit_wasm.cpp` (the browser build). JIT is not supported for 32-bit.

| | x64 | a64 | wasm |
|---|---|---|---|
| File | `jit_x64.cpp` | `jit_a64.cpp` | `jit_wasm.cpp` |
| Platform | x86-64 | AArch64 (e.g. Apple Silicon) | browser |
| Compiles | one function at a time (large ones in pieces) | same as x64 | one function at a time (never in pieces) |
| When | once a function is hot, or all on quest load with `jit_precompile` | same as x64 | same as x64 |
| On by default | yes | yes | yes, in the JSPI web build (the fallback build has no JIT) |

## Using the JIT

### Enabling

JIT compilation is on by default. On the web it only exists in the JSPI build, which browsers with `WebAssembly.Suspending` get; the fallback build for other browsers runs scripts on the interpreter (see [WASM backend](#wasm-backend)). It can be toggled with the `[ZSCRIPT] jit` config option, found in the launcher. There is also the `-jit` / `-no-jit` command line switch, or `--(no-)jit` for `run_replay_tests.py`.

### Configuration

```ini
# in zc.cfg ...

[ZSCRIPT]
# Enable for JIT-compilation of ZASM scripts.
jit = 1
# Number of background threads for compiling scripts.
# -1 to set to number of available CPU threads
# -x to set to (number of available CPU threads) / x
# 0 to only compile on the main thread (which means precompiling, see below)
jit_threads = -2
# When a function is called this many times, it is deemed "hot" and will be compiled by the JIT.
jit_hot_function_call_count = 10
# When a function loops this many times, it is deemed "hot" and will be compiled by the JIT.
jit_hot_function_loop_count = 1000
# Compile all scripts on quest load, and wait for it to finish before starting the game.
# Note that this totally ignores the above two "hot" thresholds.
jit_precompile = 0
# Cache D registers in host registers (see "D-register cache" below).
jit_cache_registers = 1
```

With `jit_threads = 0` there is no worker pool to compile hot functions in the background, so every script is precompiled on quest load, as if `jit_precompile` were on. The web build's pool has at most 8 threads.

## How the interpreter runs ZASM

`run_script` sets up the current script object (which contains the ZASM instructions), and the proper context for that script (refInfo and the stack). Normally, it then calls `run_script_int` which enters a loop that processes one ZASM instruction at a time, based on the current instruction counter `ri->pc`. Based on the instruction command, it will have 0-3 args; and may modify the state of either the stack, some "z-register", or some game object.

A "z-register" (`ffscript.cpp` calls them registers; the compiler wraps them in `VarArgument`, in `src/parser/Opcode.h`) is set/read via `set_register`/`get_register`. A handful of general purpose z-registers exist called `D` and `G` (global). All other z-registers connect some state of the game engine to ZASM - for example, `Hero->X = 123` would become the ZASM instruction `SETV LINKX 123`, so `set_register` would get called to set the player's position to the given value.

Function calls in modern quests use `CALLFUNC <destpc>` / `RETURNFUNC` (older 2.55-era compiles instead push the return pc and `GOTO <destpc>` - this is what `is_modern_function_calling()` distinguishes). `CALLFUNC` pushes the return pc onto a separate return stack (`ret_stack`, indexed by `ri->retsp`); parameters are pushed onto the data stack (`stack`, indexed by `ri->sp`), and the function addresses them and its locals relative to the `SFRAME` register (`D4`). When the function wants to return, `RETURNFUNC` pops the return pc to jump back to.

The ZASM bytecode interpreter will execute instructions until it encounters either 1) some `WaitX` command or 2) a `QUIT` command. If it somehow gets past the end of the instructions, that is treated as a `QUIT` (this should never happen). Some examples of `WaitX` are: `WaitFrame` (yield and resume on the next frame) and `WaitEvent` (wait for some game event to occur before continuing). In any case, whenever a script resumes it picks up from that last `WaitX` call with all its state and registers intact. The `refInfo` and `stack` will continue to exist for this script instance (until cleared, like for FFCs when leaving the screen).

There are 1000+ command types, and 5000+ z-registers.

## How the JIT works

### Translating ZASM to machine code

Each ZASM instruction in a script is translated to an assembly representation (using `asmjit` on native, or written directly as wasm bytes in the browser), which is then compiled into machine code and returns a function that can be called later. That function will take as parameters the refInfo, the stack, and a few other bookkeeping variables.

Only a fraction of the ZASM command types are compiled into more efficient machine code. The rest call out to the normal interpreter loop to execute. This is both because there are far too many command types, and that for many of them the overhead of the interpreter is a fraction of the time spent processing the command. Examples of commands that benefit greatly from being optimized are ones that just do math operations, set some z-register, or modify the stack.

Of the commands that _are_ compiled, if it involves reading / writing to a z-register it again will simply call out to the normal `get_register` / `set_register` functions. The big exception here is that the general-purpose z-registers (`D` and `G`) will simply address the memory directly.

### Example: `ADDR D2 D3`

`ADDR D2 D3` - Set `D2` to `D2 + D3`.

The interpreter would normally do all this work:

```c
// interpreter loop:
// (3 ops) check loop condition (scommand != 0xFFFF)
// (3 ops) check if debugger or runtime debugging is on
// (13 ops) decode instruction (command + 3 args)
// (4 ops) clear string/vector literal args
// (1 op) record current command for error messages
// (2 ops) check if per-instruction runtime debugging is on
// (5 ops) count instructions to check if script is hung
// (7 ops) jump to correct place in giant command switch
// (2 ops) call do_add
// (2 ops) jump to post-command checks
// (14 ops) check for rare conditions (early return, overflow, invalid ZASM, ...)
// (12 ops) increment the pc and check it for overflow
// (6 ops) count commands run, check if running on behalf of the JIT

// do_add:
// (8 ops) stack stuff
// (11 ops) get_register twice (inlined: D registers take a fast path)
// (1 op) do the actual adding
// (5 ops) set_register (inlined fast path)
```

In total, about 100 ops to add two numbers in general purpose z-registers. But with JIT compilation, the ZASM instruction is optimized into just 4 ops:

```x86asm
mov ecx, dword ptr [rbp+8]
mov eax, dword ptr [rbp+12]
add ecx, eax
mov dword ptr [rbp+8], ecx
```

Not only is this far fewer ops, but there are no branches. The interpreter takes 22 branches to run this one instruction (on both x64 and arm64). Most are checks that almost never fire, which the CPU predicts correctly. The expensive one is the switch's indirect jump to the `ADDR` case: its target depends on which ZASM command comes next, so the CPU has to guess it, and a wrong guess throws away the work it started on (around 15-20 cycles on modern CPUs).

Using a JIT compiler to compile ZASM like this can result in ~20x better performance for scripts that are just pure math. In practice, quests with large amounts of scripting should see a 5-10x improvement.

### D-register cache (native backends)

The native backends also cache `D0`..`D7` in host registers between flush points (see `jit_codegen_shared.h`), so runs of instructions like the above usually don't touch memory. Dirty values are written back at waits, branches, function calls and returns, and calls into the engine. A liveness analysis skips writes that no later instruction can observe; it is summarized per function so that a value kept in a register across a `CALLFUNC`, or left for the caller after a `RETURNFUNC`, counts as live (see `zasm_analyze_function_liveness`).

The cache can be turned off with `-no-jit-cache-registers` (config: `jit_cache_registers`), which is a quick way to tell whether a miscompile involves it. It is also off under `-script-runtime-debug 2`.

### Other compiled commands

`SWITCHKEY` (the value a `switch` compares against) is also kept in a register by the JIT instead of going through `get_register` / `set_register`.

`GOTO` commands will emit a `jmp` to a label that is placed at the correct place in the generated assembly.

### Compilation unit and timing

Every backend (`jit_x64.cpp`, `jit_a64.cpp`, and `jit_wasm.cpp` for the browser build) compiles scripts **per-function**, driven by `jit_shared.cpp`. Once a function is run enough (lots of calls to it, or it loops a lot internally), it will be compiled. Compilation happens in a worker pool – the main thread does not wait for it to be compiled. Once the worker pool compiles a function, the main thread can then run it when executing that function. Until then, the interpreter handles executing that function. If precompiling is enabled, all functions are compiled on quest load, before the game starts. On the native backends, very large functions are compiled in pieces (see [Large functions are compiled in pieces](#large-functions-are-compiled-in-pieces)). The wasm backend adds a step after compiling, in the browser (see the [WASM backend](#wasm-backend) section).

### Yielding and resuming

When a `WaitX` command is hit, the compiled function saves where to resume and returns; the next time it is called it picks up at the instruction after that `WaitX`. Every backend resumes from `ri->pc` (see [Resume dispatch](#resume-dispatch)).

## Native backends (x64, a64)

### Code layout

The two native backends share everything except instruction selection: `jit_shared.{h,cpp}` is the runtime driver (compilation pipeline, script/instance lifecycle, hot-function profiling, the run loop) and `jit_codegen_shared.h` is the codegen policy they must apply identically (the D-register cache and its liveness-based flush rules, the per-command emit loop, the compiled-command list). The wasm backend uses the same driver, but not the codegen policy.

### Resume dispatch

The native backends track resume points via `JittedScript::pc_to_resume_address`, and jump straight to the saved native address through an annotated indirect branch. a64 once compared `ctx->pc` against each resume point on entry instead (direct branches, so the register allocator could put fixup moves on each edge), but that chain runs on every entry, including every return from a callee: decompiled 2.50-era global scripts with thousands of call sites ran up to 30x slower than the interpreter with it, and it measured no faster even for functions with few resume points.

### Direct calls between compiled functions

The driver loop in `jit_shared.cpp` normally orchestrates every script function call: a `CALLFUNC` exits the caller's compiled code back to C++, which looks up the callee (a binary search over function start pcs), pushes the ZASM return pc, and re-enters the callee's compiled code through its prologue; every `RETURNFUNC` does the mirror image, plus a resume-address lookup to re-enter the caller mid-function. That round-trip exists because a script can `WaitX` mid-call-stack: native stack frames can't survive returning to C++, so call state has to live in the ZASM return stack instead.

But a function that can never yield (no `WaitX` transitively reachable - `ZasmFunction::may_yield`) never needs to suspend, so calls to it are emitted as plain native calls on the machine stack. The call site loads the callee's entry from the per-script `direct_entry_table`, passes the return pc in `ctx->call_pc`, sets `ctx->entry_mode = 1`, and calls. Until the callee is compiled and committed its slot holds `jit_direct_not_compiled`, a stub that rewrites `ctx` into the driver-path call the site would otherwise have made, so the slot doubles as the hot-swap point without the call site needing a branch of its own. The callee's entry pushes the ZASM return stack via `jit_direct_enter` (so the driver's view of the call chain stays exact) and skips its resume dispatch; its `RETURNFUNC` pops the return stack and returns `EXEC_RESULT_RETURN` to the call site instead of to the driver.

#### Unwinding

Everything unusual is handled by one rule: a direct call site treats any result other than `EXEC_RESULT_RETURN` as an unwind, and returns it unchanged from its own function. A `QUIT`, an error from an interpreter batch, or a driver-path fallback deep in a chain of direct calls just ripples up the native frames to the driver, which handles it exactly as if it had made the calls itself - the ZASM return stack and `ctx->pc`/`ctx->sp` were maintained identically along the way. (`ctx->pc` mirrors the driver's call/return updates deliberately: error stack traces are built from it.)

#### Guard rails

- **Recursion depth:** past 256 ZASM call frames, `jit_direct_enter` converts the call into a driver-path call, since native frames are real stack memory (`stack_limit.zs`'s 10000-deep recursion exercises this).
- **x64 `saved_regs`:** the call site preserves the single-slot `ctx->saved_regs` pinned-register save area around the nested call, which nested prologues would otherwise clobber.

  It copies the area into one stack slot shared by all of the function's call sites rather than into virtual registers: registers live across a call get spilled, each into its own never-reused slot, so with thousands of call sites the frame grew by hundreds of KB (~316 KB for the Panoply global script described below; ~7.7 KB with the shared slot).

  This matters because asmjit emits no stack probes: Windows grows a thread's stack one guard page (4 KB) at a time, so the first write into a frame larger than a page can land past the guard page and crash. Keeping call sites out of the frame size makes that far less likely, but does not rule it out.

  A big enough function can still have a frame over a page from its other spills, and is only safe if earlier, deeper calls have already committed that much stack.

#### Cost

This removes the dominant cost for call-dense scripts - std.zh-heavy quests make thousands of tiny helper calls per frame, sometimes averaging only a dozen ZASM instructions of real work per call. Across the `measure_performance.py` benchmark set, total script time dropped ~30%, with the most call-heavy replays down 60%+. The tradeoff is compile time and memory: asmjit's register allocator tracks liveness per basic block for every register live across blocks, so its cost grows with blocks x those registers, and every call site adds blocks. Call sites are kept to one extra block each (no branch on an uncompiled callee, and the non-RETURN unwinds share one exit) and keep no register of their own live across a block. Disable with `-no-jit-direct-calls`.

The worst case seen is the 149k-instruction decompiled 2.50-era global script of "The Adventure of Link and Zelda: Panoply of Calatia" (replay upload `69162522CA4C5DAD4DA0CAEDA505FBFB.qst`), with ~12.7k direct call sites. It is the quest's hot global script:

| Panoply global script, compiled whole (a64, median of 3) | without direct calls | with |
|---|---|---|
| Compile that function | ~0.6 s | ~1.1 s |
| Quest precompile | ~0.66 s | ~1.14 s |
| Peak memory (43.7k-frame replay) | ~1.41 GB | ~1.63 GB |
| Total script time | ~5.5 s | ~1.6 s |

Measured with `-jit-precompile -jit-threads 0 -jit-log` (compile times), `measure_memory.py --runs 3` (memory) and `measure_performance.py --mode jit --runs 3` (script time). Such functions are now compiled in pieces (below), which bounds that cost.

### Large functions are compiled in pieces

The memory asmjit needs to compile a function grows about linearly with its size up to ~20k ZASM instructions (at most ~5.4 KB per instruction on a64), and much faster past that, because register allocation tracks liveness per basic block for every register live across blocks. Decompiled 2.50-era global scripts can have 100k-150k instructions in one function; the Panoply script above needed 1.4 GB to compile whole.

So functions over `-jit-max-piece-size` (non-NOP) instructions, default 10000, are split into pieces of about equal size (`jit_split_function`). Each piece is compiled as if it were its own function (`JitPiece`), so a function's pieces compile in parallel. Control moves between pieces through the driver:

- **Leaving a piece** works like a GOTO out of the function: flush the D-register cache, store `ctx->pc`, and return `EXEC_RESULT_CONTINUE`.
- **Entering a piece mid-way** works like resuming after a wait: jump targets reached from other pieces are registered as resume points.

A piece starts at the start of a block where it can. A block longer than a piece (straight-line code, like a script initializing a huge array) is cut once the piece is full, so no piece is much bigger than `-jit-max-piece-size`. A piece never starts where a comparison's flags are still to be read (by the next non-NOP instruction, or by the target of a jump - only hand-written ZASM jumps to a flag reader, and then the jump and its target stay in one piece) or while `SWITCHKEY` is live, since neither survives leaving compiled code. Only a function's first piece can be entered by a direct call.

For testing, `-jit-max-piece-size 50` splits nearly every function at nearly every allowed point.

### AArch64 notes

A few AArch64-specific notes (`jit_a64.cpp`):

- If emitting a function reports an asmjit error, the function is discarded and stays interpreted (always correct, just slower for that one function). A piece too large to encode (InvalidDisplacement - see below) is treated as an expected capacity limit; any other emit error is a codegen bug and aborts under `jit_fatal_compile_errors` (which CI runs with).
- Conditional branches (`b.cond`) only reach ±1 MB, so on a64 `-jit-max-piece-size` is capped at 15000: the densest code over every script of ~1000 quests is ~57 bytes per instruction, which keeps any branch in a piece within reach. Register-allocator fixup trampolines are placed inline by commits in our asmjit fork, which also extends spill addressing past the ~16 KB encodable range via a reserved scratch register (spill frames reach ~190 KB).

## WASM backend

The browser build can't emit native machine code, so `jit_wasm.cpp` compiles ZASM to WebAssembly instead, which the browser then compiles to machine code itself. Unlike the native backends, there is no assembler library: the backend writes the wasm binary format directly (`wasm_compiler.h`).

The wasm JIT only runs in the JSPI web build (`JSPI=1 bash scripts/configure_emscripten.sh`). The fallback build for browsers without JSPI uses ASYNCIFY, and is built with `JIT_BACKEND=none`, so it runs scripts on the interpreter.

### Compilation unit and pipeline

The wasm backend plugs into the same driver as the native backends (`jit_shared.cpp`): each function is compiled on its own, once it's hot (or at quest load, with `-jit-precompile`), into its own small wasm module, and its `exec` is called by the driver loop just like a native function. Functions are never split into pieces (`jit_max_piece_size`): the browser compiles a big function fine. Compiling a function has two stages:

1. **Codegen** (ZASM -> wasm bytes) is plain C++, and runs on the engine worker pool.
2. **Browser compile** must be issued from the main thread. When a script runs, `jit_backend_prepare_commits` hands any newly generated modules to the browser in one batch (`em_compile_wasm_batch_start` -> `ZC.startCompileScriptWasmModuleBatch` in `web/zasm.js`), which compiles and instantiates them concurrently on the browser's own threads, and puts each module's `exec` in the engine's function table. Later script runs poll the batch, and commit each function once its `exec` is ready. The main thread never waits for this. For `-jit-precompile`, `jit_precompile_scripts_impl` waits for all of codegen, then compiles every module in one batch before the game starts.

A few things about the web build's threads shape this:

- **A worker's file writes (`al_trace`) are done by the main thread**, which may be busy running the game, or waiting on that worker. So JIT logs from worker threads are queued (`jit_trace`), and written by the main thread (`jit_flush_worker_logs`).
- **A new thread's web worker only starts while the main thread yields to the browser.** So `zasm_pipeline_init` waits (with `emscripten_sleep`) for the worker pool's threads to start, and the main thread waits for the pool by polling it (`WorkerPool::idle`) with `emscripten_sleep`, never by blocking.
- **The JSPI build links mimalloc.** With the default allocator (dlmalloc), a single lock covers every allocation, and codegen allocates a lot: each thread added made precompiling slower, and a background compile would stall the game's own allocations. mimalloc costs more memory (yuurand_riviere's heap peaks at 626 MB, vs 522 MB).
- Headless runs only yield to the browser every 10000 frames, so there the browser compile of a hot function finishes late.

### Module structure

Each module imports the engine's memory, function table and callbacks, and exports one function, `exec(ctx)`, with the same contract as a native function: it runs from `ctx->resume_address` (0 for the function's start), and returns an `EXEC_RESULT_*` for the driver. Hot values (the script `sp`, the refInfo/stack/global-D pointers) are kept in wasm locals, loaded from `ctx` on entry.

- **A function that never leaves for the driver mid-way** (no `WaitX`, and no calls except direct ones, below) gets real structured control flow (`compile_function_structured`; `WasmStructurer` in `wasm_structurer.h`, a dominator-tree-guided translation). Irreducible control flow falls back to the lowering below.
- **Any other function** is a loop-switch: a `loop` + `br_table` dispatch over its basic blocks (`compile_piece`), because the driver can re-enter it after a `WaitX` or after a call returns. Its resume points go in `pc_to_resume_address` (keyed by the pc before the resume point, as for the native backends), and the dispatch value is the block id + 1. Loops that never leave the function are still compiled as real wasm loops nested in the dispatch (see `detect_yielder_regions`).

Calls work like the native backends: a call to a function that can never yield is a direct call (`compile_callfunc_direct`) - a `call_indirect` through the callee's slot in `direct_entry_table`, which holds `jit_wasm_direct_not_compiled` until the callee is committed (that runs the call on the driver, to its end). The callee's entry pushes the ZASM return stack via `direct_enter`, and past 256 direct-call frames a call goes through the driver instead. A direct call site has no resume point after it, so when that call returns, the caller finishes on the interpreter, as does each caller below it when it's returned to: recursion deeper than 256 runs interpreted from there. Any other call returns `EXEC_RESULT_CALL` to the driver.

There is no JS frame between the engine and a script's compiled code, so JSPI can suspend the program with script frames on the stack. That's how `RUNGENFRZSCR` (generic frozen scripts) runs inline through the interpreter, as on the native backends: its engine frames suspend the program with the calling script's frames still on the stack.

Not every ZASM command is compiled (see `command_is_compiled`); runs of uncompiled commands call back into the interpreter in batches via `do_commands`. All engine callbacks (`get_register`/`set_register`/`do_commands`/...) are bound as **direct wasm->wasm imports** - `web/zasm.js` resolves the engine's raw exports (`rawEngineExport`) so script->engine calls skip JS entirely.

## Debugging

### Tools

`zplayer -extract-zasm game.qst` will write all scripts as text ZASM to `zscript-debug/zasm`. This is useful for reference.

These are useful config options:

- `[ZSCRIPT] jit_print_asm = 1`: for each script, write a file to `zscript-debug/zasm` printing the emitted assembly (x64 or a64)
- `[ZSCRIPT] jit_fatal_compile_errors = 1`: exit program if JIT fails to compile any script

And some CLI options:

- `-script-runtime-debug 1` to print a register/stack summary to `zscript-debug/` before executing every script
- `-script-runtime-debug 2` same as above, but prints for every instruction (lots of data)

### Debugging a JIT-only replay failure

Given a replay that fails only when using JIT, this is how you can debug what's wrong with the compiled code:

1. Have a failing replay, for example: `tests/replays/playground/maths.zplay`
1. Run `python scripts/jit_runtime_debug.py --replay_path tests/replays/playground/maths.zplay`
1. The script will run that replay w/ JIT, then w/o JIT, then show you where the registers/stack/pc first differ
1. Hopefully, you'll now see the exact instruction that results in a problem. If a fix seems non-trivial, it may be best to figure out how to create similar ZASM using a much smaller script, so you don't have to wait however long the replay is when iterating on a fix.

It can also be useful to compile only the script you're debugging in `jit_create_script_impl`: `if (script->id != {ScriptType::Player, 1}) return nullptr;`

### WASM backend

- A **native build with `-DJIT_BACKEND=wasm`** generates (but can't run) the same modules - fast, execution-free codegen validation. With `-jit-save-wasm` it writes `wasm/<qst>/<module>.wasm` next to the exe; check structure with `wasm2wat <f> --enable-threads`. This is also how to verify a codegen refactor is byte-identical.
- Each codegen strategy has an escape-hatch flag for A/B and bisection: `-jit-wasm-structured`, `-jit-wasm-structured-yielder` (the nested loops in a loop-switch), `-jit-wasm-inline-regs` (all default on).
- `-jit-run-lo N` / `-jit-run-hi N` force `run_script` calls outside `[lo, hi)` to the interpreter, for bisecting which invocation must be jitted to reproduce a divergence. The gate is only active when `-jit-run-hi` is set (its default of -1 disables it), so `-jit-run-lo` alone has no effect. Once the failing call is found, `-jit-run-log-call N` logs the replay frame and script that `run_script` call N runs on. `scripts/jit_runtime_debug.py` automates this against a native no-jit baseline and works on web replays (pass `--baseline_build_folder build/Release` to speed up baseline collection).

## Flags reference

Boolean flags can be negated with a `-no` prefix (e.g. `-no-jit-wasm-structured`). Where a config option is listed, the flag overrides it.

| Flag | Backend | Default | What it does |
|---|---|---|---|
| `-jit` | all | on | Enable the JIT. Config: `[ZSCRIPT] jit` |
| `-jit-precompile` | all | off | Compile every script on quest load. Config: `jit_precompile` |
| `-jit-threads N` | all | -2 | Compile threads; 0 implies `-jit-precompile`. Config: `jit_threads` |
| `-jit-hot-function-call-count N` | all | 10 | Config: `jit_hot_function_call_count` |
| `-jit-hot-function-loop-count N` | all | 1000 | Config: `jit_hot_function_loop_count` |
| `-jit-log` | all | off (on in CI) | Log compilation to `allegro.log`. Config: `jit_log` |
| `-jit-print-asm` | native | off | Config: `jit_print_asm` |
| `-jit-fatal-compile-errors` | all | off | Config: `jit_fatal_compile_errors` |
| `-jit-cache-registers` | native | on | [D-register cache](#d-register-cache-native-backends). Config: `jit_cache_registers` |
| `-jit-direct-calls` | native | on | [Direct calls between compiled functions](#direct-calls-between-compiled-functions) |
| `-jit-max-piece-size N` | native | 10000 | [Split larger functions into pieces](#large-functions-are-compiled-in-pieces) of at most this many instructions (at most 15000 on a64) |
| `-jit-save-wasm` | wasm | off | Write modules to `wasm/<qst>/` |
| `-jit-wasm-structured`, `-jit-wasm-structured-yielder`, `-jit-wasm-inline-regs` | wasm | on | Codegen escape hatches |
| `-jit-run-lo N` / `-jit-run-hi N` | all | off | Bisect which `run_script` call must be jitted |
| `-jit-run-log-call N` | all | off | Log the frame and script of `run_script` call N |
| `-script-runtime-debug 1\|2` | all | off | Register/stack dumps per script (1) or per instruction (2) |

## Learning materials

### x64 assembly

- https://web.stanford.edu/class/cs107/guide/x86-64.html
- https://www.cs.uaf.edu/2017/fall/cs301/lecture/09_11_registers.html
- https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
- https://www.cavestory.org/guides/csasm/guide/redefine_imul_idiv.html
- https://github.com/FFmpeg/asm-lessons/blob/main/lesson_01/index.md

### asmjit

- https://eli.thegreenplace.net/2017/adventures-in-jit-compilation-part-2-an-x64-jit/
- https://gitter.im/asmjit/asmjit?at=5fd5247b91e8cb3e8cf92b62
