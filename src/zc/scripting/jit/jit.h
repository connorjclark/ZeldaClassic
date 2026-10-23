// See docs/jit.md for an overview of the JIT (Just-in-time) compiler for ZScript.

#ifndef ZC_JIT_H_
#define ZC_JIT_H_

#include "base/compiler.h"
#include "core/zdefs.h"
#include "components/zasm/pc.h"
#include <functional>
#include <vector>

#define JIT_NONE 0
#define JIT_X64 1
#define JIT_WASM 2
#define JIT_A64 3

#if ZC_JIT_BACKEND == JIT_NONE
	#include "zc/scripting/jit/jit_none.h"
#elif ZC_JIT_BACKEND == JIT_X64
	#include "zc/scripting/jit/jit_x64.h"
#elif ZC_JIT_BACKEND == JIT_WASM
	#include "zc/scripting/jit/jit_wasm.h"
#elif ZC_JIT_BACKEND == JIT_A64
	#include "zc/scripting/jit/jit_a64.h"
#endif

ZC_FORMAT_PRINTF(1, 2)
void jit_printf(const char *format, ...);
ZC_FORMAT_PRINTF(1, 2)
void jit_error(const char *format, ...);
bool jit_is_enabled();
void jit_set_enabled(bool enabled);
bool jit_log_is_enabled();
bool jit_is_use_cached_regs_enabled();
bool jit_should_precompile();
JittedScriptInstance* jit_create_script_instance(script_data* script, refInfo* ri);
void jit_profiler_increment_function_back_edge(JittedScriptInstance* j_instance, pc_t pc);
void jit_startup(bool precompile);
void jit_shutdown();

// The following vary per backend.

void jit_startup_impl();
JittedScript* jit_compile_script(zasm_script* script);
// Precompile every script, if the backend has its own way to. Returns false if
// not (the caller then compiles serially or via the engine worker pool). The
// wasm backend implements this, as each module also has to be compiled by the
// browser, which must happen on the main thread: setup and codegen run on the
// engine worker pool, then the main thread compiles every module in one batch.
// on_compiled is called on the calling thread for every script, in no
// particular order.
bool jit_precompile_scripts_impl(const std::vector<zasm_script*>& scripts, const std::function<void(zasm_script*, JittedScript*)>& on_compiled);
JittedScriptInstance* jit_create_script_impl(script_data* script, refInfo* ri, JittedScript* j_script);
int jit_run_script(JittedScriptInstance* j_instance);
void jit_release(JittedScript* j_script);

#endif
