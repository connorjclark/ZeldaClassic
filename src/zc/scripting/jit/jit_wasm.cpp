// See docs/jit.md for implementation details.

// Many useful resources
// https://webassembly.github.io/spec/core/binary/modules.html
// https://pengowray.github.io/wasm-ops/
// https://github.com/WebAssembly/design/blob/main/BinaryEncoding.md
// https://github.com/sunfishcode/wasm-reference-manual/blob/master/WebAssembly.md
// https://webassembly.github.io/wabt/demo/wasm2wat/
// https://webassembly.github.io/wabt/demo/wat2wasm/
// https://openhome.cc/eGossip/WebAssembly/Block.html
// https://samrat.me/blog/2020-03-29-webassembly-control-instr-examples
// https://musteresel.github.io/posts/2020/01/webassembly-text-br_table-example.html
// https://stackoverflow.com/questions/63880579/how-to-convert-a-control-flow-graph-back-into-its-source-code-e-g-c-c
// https://stackoverflow.com/questions/8866032/flattening-a-control-flow-graph-to-structured-code

// Useful command for quickly verifying valid WASM is generated:
/*
	first, configure build w/ JIT_BACKEND=wasm

	cmake --build build --config Debug -t  zplayer &&
	rm -rf build/Debug/wasm/playground/playground.qst &&
	./build/Debug/zplayer -replay $PWD/tests/replays/playground/maths.zplay -headless -jit -jit-save-wasm -jit-precompile -frame 0 -replay-exit-when-done &&
	wasm2wat build/Debug/wasm/playground.qst/<script>-<function>-<start pc>.wasm --enable-threads --generate-names
*/

#include "zc/scripting/jit/jit_wasm.h"
#include "core/qrs.h"

#include "allegro/debug.h"
#include "allegro/file.h"
#include "base/check.h"
#include "base/general.h"
#include "base/zapp.h"
#include "core/zdefs.h"
#include "components/zasm/pc.h"
#include "components/zasm/table.h"
#include "zc/ffscript.h"
#include "zc/scripting/jit/jit.h"
#include "zc/script_debug.h"
#include "zc/wasm_compiler.h"
#include "zc/wasm_structurer.h"
#include "zc/zasm_pipeline.h"
#include "zc/zasm_utils.h"
#include "zc/zelda.h"
#include "components/zasm/serialize.h"
#include "components/worker_pool/worker_pool.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

struct CompilationState
{
	WasmAssembler* wasm;
	JittedScript* j_script;
	bool runtime_debugging;
	// When true, ri->pc is kept current before engine calls, for the stack trace of an error they
	// may log. Gated on stack traces being possible, to avoid the per-call cost otherwise.
	bool maintain_call_stack;
	// Inline side-effect-free hot engine registers (SWITCHKEY, the
	// GLOBALRAM/SCRIPTRAM array accessors) instead of calling get/set_register.
	bool inline_regs;

	pc_t pc;
	// The piece being compiled (see JitPiece).
	pc_t start_pc;
	pc_t final_pc;

	uint8_t f_idx_do_commands;
	uint8_t f_idx_direct_enter;
	uint8_t f_idx_direct_retstack_pop;
	// The type of exec(ctx), for direct calls into other compiled functions.
	uint32_t exec_type;
	uint8_t f_idx_get_register;
	uint8_t f_idx_set_register;
	uint8_t f_idx_set_guarded_register;
	uint8_t f_idx_runtime_debug;
	uint8_t f_idx_log_error;
	uint8_t f_idx_pod_read;
	uint8_t f_idx_pod_write;
	uint8_t f_idx_allocatemem;
	uint8_t f_idx_writepodarr;
	uint8_t f_idx_class_read;
	uint8_t f_idx_class_write;
	uint8_t f_idx_string_compare;
	uint8_t f_idx_ref_change;
	uint8_t f_idx_mark_type_stack;
	uint8_t f_idx_arraysize;
};

#ifdef __EMSCRIPTEN__

// Starts compiling many modules, and returns a batch id: the browser compiles them in parallel on
// its own threads. The module bytes are copied before this returns.
EM_JS(int, em_compile_wasm_batch_start_, (int count, uintptr_t names, uintptr_t ptrs, uintptr_t sizes), {
	return ZC.startCompileScriptWasmModuleBatch(count, names, ptrs, sizes);
});
static int em_compile_wasm_batch_start(int count, const char** names, void** ptrs, uint32_t* sizes)
{
	return em_compile_wasm_batch_start_(count, (uintptr_t)names, (uintptr_t)ptrs, (uintptr_t)sizes);
}

// If the batch has finished compiling, writes each module's exec function (its function table
// slot, 0 = failure) to out_ids and returns true.
EM_JS(bool, em_compile_wasm_batch_poll_, (int batch_id, uintptr_t out_ids), {
	return ZC.pollCompileScriptWasmModuleBatch(batch_id, out_ids);
});
static bool em_compile_wasm_batch_poll(int batch_id, int* out_ids)
{
	return em_compile_wasm_batch_poll_(batch_id, (uintptr_t)out_ids);
}

// Discards a batch: its modules are released once they finish compiling.
EM_JS(void, em_compile_wasm_batch_cancel_, (int batch_id), {
	ZC.cancelCompileScriptWasmModuleBatch(batch_id);
});
static void em_compile_wasm_batch_cancel(int batch_id)
{
	em_compile_wasm_batch_cancel_(batch_id);
}

EM_JS(bool, em_destroy_wasm_module_, (int id), {
	return ZC.destroyScriptWasmModule(id);
});
static bool em_destroy_wasm_module(int id)
{
	return em_destroy_wasm_module_(id);
}

#else

static int em_compile_wasm_batch_start([[maybe_unused]] int count, [[maybe_unused]] const char** names, [[maybe_unused]] void** ptrs, [[maybe_unused]] uint32_t* sizes)
{
	return 0;
}

static bool em_compile_wasm_batch_poll([[maybe_unused]] int batch_id, [[maybe_unused]] int* out_ids)
{
	return false;
}

static void em_compile_wasm_batch_cancel([[maybe_unused]] int batch_id)
{
}


static bool em_destroy_wasm_module([[maybe_unused]] int id)
{
	return false;
}

#endif

// Codegen of a function logs to this buffer, written out as one piece when done (see
// jit_backend_compile_function).
static thread_local std::string* codegen_log;

static void codegen_trace(const std::string& str)
{
	if (codegen_log)
		codegen_log->append(str);
	else
		jit_trace(str);
}

static void error(const zasm_script* script, std::string str, bool expected = false)
{
	str = fmt::format("failed to compile zasm chunk: {} id: {}\nerror: {}\n",
					  script->name, script->id, str);

	codegen_trace(str);

	if (DEBUG_JIT_EXIT_ON_COMPILE_FAIL && !expected)
	{
		abort();
	}
}

// Each piece of a function (see JitPiece) is compiled to a module exporting one function,
// exec(ctx) -> JitExecResult, which the driver in jit_shared.cpp calls the same way it calls the
// native backends' code. Its locals:
//
// - 0: ctx, the JittedExecutionContext* parameter.
// - 1: scratch. 2/3: the comparison operand snapshots (2 doubles as a second scratch for
//   two-operand commands, never live at the same time as a comparison snapshot, since a
//   comparison's consumers immediately follow it).
// - 4: sp. Kept in a local, which the optimizing tier keeps in a register, and written back to
//   ctx->sp on every exit (see emit_exit).
// - 5..9: the stack base, ri, global_d, script_d and j_instance pointers, loaded once from ctx
//   (emit_exec_prologue) and never changed.
// - 10: the dispatch target of the loop-switch lowering (cfg block id + 1).
// - 11: ctx->entry_mode as on entry: nonzero if direct-called (see compile_callfunc_direct).
//
// Nothing lives in module globals: a piece can be re-entered (a script running another script
// whose code is the same piece) while it is executing.
constexpr uint8_t l_idx_ctx = 0;
constexpr uint8_t l_idx_scratch = 1;
constexpr uint8_t l_idx_scratch2 = 2;
constexpr uint8_t l_idx_cmp1 = 2;
constexpr uint8_t l_idx_cmp2 = 3;
constexpr uint8_t l_idx_sp_local = 4;
constexpr uint8_t l_idx_stack_local = 5;
constexpr uint8_t l_idx_ri_local = 6;
constexpr uint8_t l_idx_global_d_local = 7;
constexpr uint8_t l_idx_script_d_local = 8;
constexpr uint8_t l_idx_j_instance_local = 9;
constexpr uint8_t l_idx_target_block = 10;
constexpr uint8_t l_idx_entry_mode = 11;
constexpr int num_exec_locals = 11; // Not counting the ctx parameter.

// Past this many ZASM call frames, a call to a function that can't yield goes through the driver
// instead of being made directly - direct calls nest wasm frames, of which the browser has a
// limited stack (stack_limit.zs recurses 10000 deep).
constexpr int max_direct_call_depth = 256;

// ctx->registers is ri->d, and the engine reads refInfo fields off ri (see get_z_register).
static_assert(offsetof(refInfo, d) == 4);
#ifdef __EMSCRIPTEN__
static_assert(sizeof(void*) == 4, "the wasm JIT assumes wasm32 pointers");
#endif

static void emit_get_sp(CompilationState& state)
{
	state.wasm->emitLocalGet(l_idx_sp_local);
}

// Pops the new sp value from the wasm operand stack.
static void emit_set_sp(CompilationState& state)
{
	state.wasm->emitLocalSet(l_idx_sp_local);
}

static void emit_get_stack_base(CompilationState& state)
{
	state.wasm->emitLocalGet(l_idx_stack_local);
}

static void emit_get_ri(CompilationState& state)
{
	state.wasm->emitLocalGet(l_idx_ri_local);
}

static void emit_get_global_d(CompilationState& state)
{
	state.wasm->emitLocalGet(l_idx_global_d_local);
}

static void emit_get_script_d(CompilationState& state)
{
	state.wasm->emitLocalGet(l_idx_script_d_local);
}

static void emit_get_j_instance(CompilationState& state)
{
	state.wasm->emitLocalGet(l_idx_j_instance_local);
}

static void emit_load_ctx(CompilationState& state, size_t offset)
{
	state.wasm->emitLocalGet(l_idx_ctx);
	state.wasm->emitI32Load(offset);
}

// ctx->field = the value `emit_value` pushes.
template <typename F>
static void emit_store_ctx(CompilationState& state, size_t offset, F&& emit_value)
{
	state.wasm->emitLocalGet(l_idx_ctx);
	emit_value();
	state.wasm->emitI32Store(offset);
}

static void emit_store_ctx_const(CompilationState& state, size_t offset, int32_t value)
{
	emit_store_ctx(state, offset, [&]{ state.wasm->emitI32Const(value); });
}

// Loads the locals exec(ctx) keeps its state in, and the dispatch target: 0 to start at the
// piece's start, or the dispatch value of a resume point (see pc_to_resume_address).
static void emit_exec_prologue(CompilationState& state)
{
	auto& wasm = *state.wasm;
	emit_load_ctx(state, offsetof(JittedExecutionContext, sp));
	wasm.emitLocalSet(l_idx_sp_local);
	emit_load_ctx(state, offsetof(JittedExecutionContext, stack_base));
	wasm.emitLocalSet(l_idx_stack_local);
	emit_load_ctx(state, offsetof(JittedExecutionContext, registers));
	wasm.emitI32Const(offsetof(refInfo, d));
	wasm.emitI32Sub();
	wasm.emitLocalSet(l_idx_ri_local);
	emit_load_ctx(state, offsetof(JittedExecutionContext, global_registers));
	wasm.emitLocalSet(l_idx_global_d_local);
	emit_load_ctx(state, offsetof(JittedExecutionContext, script_registers));
	wasm.emitLocalSet(l_idx_script_d_local);
	emit_load_ctx(state, offsetof(JittedExecutionContext, j_instance));
	wasm.emitLocalSet(l_idx_j_instance_local);
	emit_load_ctx(state, offsetof(JittedExecutionContext, resume_address));
	wasm.emitLocalSet(l_idx_target_block);

	// A direct call always enters at the start, and the callee pushes the return pc itself (the
	// driver does for a call it makes). ctx->resume_address is stale then.
	emit_load_ctx(state, offsetof(JittedExecutionContext, entry_mode));
	wasm.emitLocalTee(l_idx_entry_mode);
	wasm.emitIf();
	{
		wasm.emitLocalGet(l_idx_ctx);
		wasm.emitI32Const(state.start_pc);
		wasm.emitCall(state.f_idx_direct_enter);
		wasm.emitI32Const(0);
		wasm.emitLocalSet(l_idx_target_block);
	}
	wasm.emitEnd();
}

// Returns `result` to the driver. Every exit writes sp back to ctx->sp, like the native backends.
static void emit_exit(CompilationState& state, JitExecResult result)
{
	emit_store_ctx(state, offsetof(JittedExecutionContext, sp), [&]{ emit_get_sp(state); });
	state.wasm->emitI32Const(result);
	state.wasm->emitReturn();
}

// A RETURNFUNC. The driver pops the return pc for a call it made; a direct-called function pops it
// itself (see compile_callfunc_direct).
static void emit_return(CompilationState& state)
{
	state.wasm->emitLocalGet(l_idx_entry_mode);
	state.wasm->emitIf();
	state.wasm->emitCall(state.f_idx_direct_retstack_pop);
	state.wasm->emitEnd();
	emit_exit(state, EXEC_RESULT_RETURN);
}

// Continues at `pc` in the driver: a jump out of this piece, or falling off its end.
static void emit_exit_continue(CompilationState& state, pc_t pc)
{
	emit_store_ctx_const(state, offsetof(JittedExecutionContext, pc), pc);
	emit_exit(state, EXEC_RESULT_CONTINUE);
}

// Stops the script with the RUNSCRIPT_* code `ret_code`, at `pc`.
static void emit_exit_stop(CompilationState& state, int32_t ret_code, pc_t pc)
{
	emit_store_ctx_const(state, offsetof(JittedExecutionContext, ret_code), ret_code);
	emit_store_ctx_const(state, offsetof(JittedExecutionContext, pc), pc);
	emit_exit(state, EXEC_RESULT_EXIT);
}

// Stops the script with the RUNSCRIPT_* code the interpreter returned (in l_idx_scratch), where
// the interpreter left ri->pc (like after a Hero->Lift() of itself) - see the native backends'
// ret_if_not_ok.
static void emit_exit_interpreter_result(CompilationState& state)
{
	emit_store_ctx(state, offsetof(JittedExecutionContext, ret_code), [&]{ state.wasm->emitLocalGet(l_idx_scratch); });
	emit_store_ctx(state, offsetof(JittedExecutionContext, pc), [&]{
		emit_get_ri(state);
		state.wasm->emitI32Load(offsetof(refInfo, pc));
	});
	emit_exit(state, EXEC_RESULT_EXIT);
}

static void add_sp(CompilationState& state, int delta)
{
	emit_get_sp(state);
	state.wasm->emitI32Const(delta);
	state.wasm->emitI32Add();
	emit_set_sp(state);
}

static void check_sp(CompilationState& state)
{
	auto& wasm = *state.wasm;

	emit_get_sp(state);
	wasm.emitI32Const(MAX_STACK_SIZE);
	wasm.emitI32GeU();

	wasm.emitIf();
	// The stack-overflow error's innermost stack frame is the command that overflowed (matching
	// the interpreter), not a stale pc.
	emit_get_ri(state);
	wasm.emitI32Const(state.pc);
	wasm.emitI32Store(offsetof(refInfo, pc));
	emit_exit_stop(state, RUNSCRIPT_JIT_STACK_OVERFLOW, state.pc);
	wasm.emitEnd();
}

static void get_z_register(CompilationState& state, int r)
{
	if (r >= D(0) && r < D(INITIAL_D))
	{
		emit_get_ri(state);
		state.wasm->emitI32Load(4 + r*4);
	}
	else if (r >= GD(0) && r < GD(MAX_GLOBAL_VARIABLES))
	{
		emit_get_global_d(state);
		state.wasm->emitI32Load((r - GD(0)) * 4); // game->global_d[]
	}
	else if (r >= SCRIPT_INST_VARS(0) && r < SCRIPT_INST_VARS(MAX_SCRIPT_INST_VARIABLES))
	{
		emit_get_script_d(state);
		state.wasm->emitI32Load((r - SCRIPT_INST_VARS(0)) * 4); // ri->script_d[]
	}
	else if (r == SP)
	{
		emit_get_sp(state);
		state.wasm->emitI32Const(10000);
		state.wasm->emitI32Mul();
	}
	else if (r == SP2)
	{
		emit_get_sp(state);
	}
	else if (r == SWITCHKEY && state.inline_regs)
	{
		// A plain refInfo field with no side effects (the x64 backend inlines
		// it too); this is the hottest engine register in switch-heavy scripts.
		emit_get_ri(state);
		state.wasm->emitI32Load(offsetof(refInfo, switchkey));
	}
	else if (auto ref = state.inline_regs ? script_ref_for_register_jit(r) : std::nullopt)
	{
		// The REF* registers are plain refInfo fields too.
		emit_get_ri(state);
		state.wasm->emitI32Load(offsetof(refInfo, refs) + (int)ref->ref * 4);
		if (ref->scaled)
		{
			state.wasm->emitI32Const(10000);
			state.wasm->emitI32Mul();
		}
	}
	else if ((r == GLOBALRAM || r == SCRIPTRAM || r == GLOBALRAMD || r == SCRIPTRAMD) && state.inline_regs)
	{
		// Old-style array reads (arrayptr in D0/rINDEX, element in D1/rINDEX2):
		// go straight to pod_read - the same ArrayH::getElement the engine's
		// get_register would reach, minus the giant register switch - with
		// no_neg set, because these registers never allow negative indices
		// (unlike the READPODARRAY family, which honors the QR).
		get_z_register(state, rINDEX);
		if (r == GLOBALRAMD || r == SCRIPTRAMD)
			state.wasm->emitI32Const(0);
		else
		{
			get_z_register(state, rINDEX2);
			state.wasm->emitI32Const(10000);
			state.wasm->emitI32DivS();
		}
		state.wasm->emitI32Const(state.pc);
		state.wasm->emitI32Const(1); // no_neg
		state.wasm->emitCall(state.f_idx_pod_read);
	}
	else
	{
		if (does_register_use_stack(r))
		{
			// ri->sp = sp
			emit_get_ri(state);
			emit_get_sp(state);
			state.wasm->emitI32Store(4*9); // ri->sp
		}

		// ri->pc = state.pc; needed for an accurate stack trace should the engine
		// call log an error. create_stack_trace is its only consumer, so skip it
		// when stack traces won't be shown - except for reads of the PC register,
		// which returns ri->pc itself (the interpreter and x64 backend keep it
		// current, so a stale value here would be a web-only divergence).
		if (state.maintain_call_stack || r == PC)
		{
			emit_get_ri(state);
			state.wasm->emitI32Const(state.pc);
			state.wasm->emitI32Store(0); // ri->pc
		}

		state.wasm->emitI32Const(r);
		state.wasm->emitCall(state.f_idx_get_register);
	}
}

// fn is a template (not std::function) so the per-command capturing lambdas
// don't heap-allocate - this is called for nearly every compiled instruction,
// and the allocations dominated compile-time profiles.
template <typename F>
static void set_z_register(CompilationState& state, int r, F&& fn)
{
	if (r >= D(0) && r < D(INITIAL_D))
	{
		emit_get_ri(state);
		fn();
		state.wasm->emitI32Store(4 + r*4);
	}
	else if (r >= GD(0) && r < GD(MAX_GLOBAL_VARIABLES))
	{
		emit_get_global_d(state);
		fn();
		state.wasm->emitI32Store((r - GD(0)) * 4); // game->global_d[]
	}
	else if (r >= SCRIPT_INST_VARS(0) && r < SCRIPT_INST_VARS(MAX_SCRIPT_INST_VARIABLES))
	{
		emit_get_script_d(state);
		fn();
		state.wasm->emitI32Store((r - SCRIPT_INST_VARS(0)) * 4); // ri->script_d[]
	}
	else if (r == SWITCHKEY && state.inline_regs)
	{
		emit_get_ri(state);
		fn();
		state.wasm->emitI32Store(offsetof(refInfo, switchkey));
	}
	else if (auto ref = state.inline_regs ? script_ref_for_register_jit(r) : std::nullopt)
	{
		emit_get_ri(state);
		fn();
		if (ref->scaled)
		{
			state.wasm->emitI32Const(10000);
			state.wasm->emitI32DivS();
		}
		state.wasm->emitI32Store(offsetof(refInfo, refs) + (int)ref->ref * 4);
	}
	else if ((r == GLOBALRAM || r == SCRIPTRAM || r == GLOBALRAMD || r == SCRIPTRAMD) && state.inline_regs)
	{
		// See the matching get_z_register branch. Writes through these
		// registers hold plain values, never object references (type = none),
		// matching the engine's set_register lowering.
		get_z_register(state, rINDEX);
		if (r == GLOBALRAMD || r == SCRIPTRAMD)
			state.wasm->emitI32Const(0);
		else
		{
			get_z_register(state, rINDEX2);
			state.wasm->emitI32Const(10000);
			state.wasm->emitI32DivS();
		}
		fn();
		state.wasm->emitI32Const((int)script_object_type::none);
		state.wasm->emitI32Const(state.pc);
		state.wasm->emitI32Const(1); // no_neg
		state.wasm->emitCall(state.f_idx_pod_write);
	}
	else
	{
		if (does_register_use_stack(r))
		{
			// ri->sp = sp
			emit_get_ri(state);
			emit_get_sp(state);
			state.wasm->emitI32Store(4*9); // ri->sp
		}

		// ri->pc = state.pc; needed for an accurate stack trace should the engine
		// call log an error. create_stack_trace is its only consumer, so skip it
		// when stack traces won't be shown.
		if (state.maintain_call_stack)
		{
			emit_get_ri(state);
			state.wasm->emitI32Const(state.pc);
			state.wasm->emitI32Store(0); // ri->pc
		}

		state.wasm->emitI32Const(r);
		fn();

		// Some registers have an extra check when writing to them.
		if (is_guarded_script_register(r))
		{
			state.wasm->emitI32Const(state.pc); // Needed for accurate stack trace should an error occur.
			state.wasm->emitCall(state.f_idx_set_guarded_register);
		}
		else
		{
			state.wasm->emitCall(state.f_idx_set_register);
		}
	}
}

// Defer to the ZASM command interpreter for 1+ commands.
// is_wait: the command is a wait, so exit to the driver if it waits.
// Any command the interpreter can't finish normally (see instruction_could_return_not_ok) exits
// with what it returned - including the suspends, like an lweapon script lifting itself, for which
// the interpreter leaves ri->pc where the script resumes.
static void compile_command_interpreter(CompilationState& state, const zasm_script* script, int pc, int count, bool is_wait = false, bool is_suspend = false)
{
	auto& wasm = *state.wasm;

	// TODO: fast path for these commands?
	bool needs_sp = false;
	for (int j = 0; j < count; j++)
	{
		int index = pc + j;
		if (index >= script->size)
			break;

		// RUNGENFRZSCR runs engine frames, which can scan this script's stack (GC).
		if (script->zasm[index].command == STORE_OBJECT || script->zasm[index].command == REF_REMOVE || script->zasm[index].command == RUNGENFRZSCR)
		{
			needs_sp = true;
			break;
		}
	}
	if (needs_sp)
	{
		// ri->sp = sp
		emit_get_ri(state);
		emit_get_sp(state);
		wasm.emitI32Store(offsetof(refInfo, sp));
	}

	emit_get_j_instance(state);
	wasm.emitI32Const(pc);
	wasm.emitI32Const(count);
	emit_get_sp(state);
	wasm.emitCall(state.f_idx_do_commands);

	bool moves_sp = false;
	for (int j = 0; j < count && pc + j < (int)script->size; j++)
	{
		if (command_moves_sp(script->zasm[pc + j].command))
			moves_sp = true;
	}
	if (moves_sp)
	{
		// See command_moves_sp.
		emit_get_ri(state);
		wasm.emitI32Load(offsetof(refInfo, sp));
		emit_set_sp(state);
	}

	if (is_wait)
	{
		// RUNSCRIPT_STOPPED means it is actually waiting (a WAITFRAMESR of 0 doesn't): exit, to
		// resume after it (see pc_to_resume_address).
		wasm.emitLocalTee(l_idx_scratch);
		wasm.emitIf();
		{
			wasm.emitLocalGet(l_idx_scratch);
			wasm.emitI32Const(RUNSCRIPT_STOPPED);
			wasm.emitI32Eq();
			wasm.emitIf();
			emit_store_ctx_const(state, offsetof(JittedExecutionContext, ret_code), RUNSCRIPT_OK);
			emit_store_ctx_const(state, offsetof(JittedExecutionContext, pc), pc + 1);
			emit_exit(state, EXEC_RESULT_EXIT);
			wasm.emitEnd();
			emit_exit_interpreter_result(state);
		}
		wasm.emitEnd();
		return;
	}

	bool could_return_not_ok = is_suspend;
	for (int j = 0; j < count; j++)
	{
		int index = pc + j;
		if (index >= script->size)
			break;

		if (instruction_could_return_not_ok(script->zasm[index]))
		{
			could_return_not_ok = true;
			break;
		}
	}

	if (could_return_not_ok)
	{
		wasm.emitLocalTee(l_idx_scratch);
		wasm.emitIf();
		emit_exit_interpreter_result(state);
		wasm.emitEnd();
	}
	else
	{
		wasm.emitDrop();
	}
}

static bool command_is_compiled(int command)
{
	if (command_is_wait(command))
		return true;

	// Comparison producers and consumers are critical to control flow. That includes STRCMPR/
	// STRICMPR (string switch): a consumer following an interpreted STRCMPR would have no result
	// to test.
	if (command_uses_comparison_result(command) || command_sets_comparison_result(command))
		return true;

	switch (command)
	{
	// These commands are critical to control flow.
	case GOTO:
	case GOTOTABLE:
	case GOTORANGES:
	case QUIT:
	case CALLFUNC:
	case RETURNFUNC:
	// Runs through the interpreter on its own (see compile_function), not batched
	// with other commands into a do_commands() call.
	case RUNGENFRZSCR:

	// These commands modify the stack pointer, which is just a local copy. If these commands
	// were not compiled, then ri->sp would have to be restored after compile_command_interpreter.
	case POP:
	case POPARGS:
	case PUSHR:
	case PUSHV:
	case PUSHARGSR:
	case PUSHARGSV:

	// These can be commented out to instead run interpreted. Useful for
	// singling out problematic instructions.
	case ABS:
	case ADDR:
	case ADDV:
	case ANDR:
	case ANDV:
	case CASTBOOLF:
	case CASTBOOLI:
	case CEILING:
	case DIVR:
	case DIVV:
	case FLOOR:
	case LOAD:
	case LOADD:
	case LOADI:
	case MAXR:
	case MAXV:
	case MINR:
	case MINV:
	case MODR:
	case MODV:
	case MULTR:
	case MULTV:
	case NOP:
	case ORR:
	case ORR32:
	case ORV:
	case ORV32:
	case PEEK:
	case SETR:
	case SETV:
	case STORE:
	case STORED:
	case STOREI:
	case SUBR:
	case SUBV:
	case SUBV2:
	case BITNOT:
	case BITNOT32:
	case LSHIFTV:
	case LSHIFTV32:
	case RSHIFTV:
	case RSHIFTV32:
	case LSHIFTR:
	case LSHIFTR32:
	case RSHIFTR:
	case RSHIFTR32:
	case XORR:
	case XORV:
	case XORR32:
	case XORV32:
	case READPODARRAYR:
	case READPODARRAYV:
	case WRITEPODARRAYRR:
	case WRITEPODARRAYRV:
	case WRITEPODARRAYVR:
	case WRITEPODARRAYVV:
	case WRITEPODARRAY:
	case ALLOCATEMEMV:
	case ZCLASS_READ:
	case ZCLASS_WRITE:
	case REF_INC:
	case REF_DEC:
	case MARK_TYPE_STACK:
	case ARRAYSIZE:
		return true;
	}

	return false;
}

static bool instruction_is_compiled(const ffscript& op)
{
	return command_is_compiled(op.command) && !jit_instruction_runs_in_interpreter(op);
}

static bool command_is_goto_consumer(int command)
{
	return command == GOTOCMP || command == GOTOTRUE || command == GOTOFALSE ||
		   command == GOTOMORE || command == GOTOLESS;
}

// A comparison command (COMPARER/COMPAREV/COMPAREV2) is fused with the run of
// commands that consume its result. A single comparison can feed MULTIPLE
// consumers: the optimizer emits e.g. `COMPAREV D2 N; SETCMP D2; GOTOCMP T`,
// where the boolean is both stored to a register AND branched on, all from the
// one compare. Each consumer re-derives the comparison from the operands.
struct CmpGroup
{
	pc_t cmp_pc;
	bool arg1_is_imm;
	bool arg2_is_imm;
	// A STRCMPR/STRICMPR producer: the operands are the engine's strcmp result
	// (captured once into l_idx_cmp1) and 0, which is how check_cmp reads it.
	bool is_strcmp;
	// With more than one real consumer, snapshot the operand values into the
	// reserved locals up-front, so a SETx consumer overwriting an operand
	// register can't corrupt a later one.
	bool capture;
	std::vector<pc_t> consumers; // the NOP/SETx/GOTOx run following cmp_pc
};

static CmpGroup analyze_comparison(const zasm_script* script, pc_t i)
{
	CmpGroup g{};
	g.cmp_pc = i;

	int command = script->zasm[i].command;
	g.is_strcmp = command == STRCMPR || command == STRICMPR;
	g.arg1_is_imm = false;
	g.arg2_is_imm = command == COMPAREV || command == COMPAREV2;
	if (command == COMPAREV2)
		std::swap(g.arg1_is_imm, g.arg2_is_imm);

	for (pc_t j = i + 1; j < (pc_t)script->size; j++)
	{
		int c = script->zasm[j].command;
		if (c == NOP || command_uses_comparison_result(c))
			g.consumers.push_back(j);
		else
			break;
	}

	int real_consumers = 0;
	for (pc_t j : g.consumers)
		if (script->zasm[j].command != NOP)
			real_consumers++;
	g.capture = g.is_strcmp || real_consumers > 1;

	return g;
}

// Emits the operand snapshot for a capturing group; called once, at the compare.
static void emit_cmp_capture(CompilationState& state, const zasm_script* script, const CmpGroup& g)
{
	if (!g.capture)
		return;
	WasmAssembler& wasm = *state.wasm;
	int arg1 = script->zasm[g.cmp_pc].arg1;
	int arg2 = script->zasm[g.cmp_pc].arg2;
	if (g.is_strcmp)
	{
		// The snapshot is the comparison result itself: the engine compares the
		// two string arrays once, and every consumer tests it against 0.
		get_z_register(state, arg1);
		get_z_register(state, arg2);
		wasm.emitI32Const(script->zasm[g.cmp_pc].command == STRICMPR ? 1 : 0);
		wasm.emitI32Const(g.cmp_pc);
		wasm.emitCall(state.f_idx_string_compare);
		wasm.emitLocalSet(l_idx_cmp1);
		return;
	}
	if (!g.arg1_is_imm) { get_z_register(state, arg1); wasm.emitLocalSet(l_idx_cmp1); }
	if (!g.arg2_is_imm) { get_z_register(state, arg2); wasm.emitLocalSet(l_idx_cmp2); }
}

// Pushes both comparison operands for one consumer.
static void emit_cmp_operands(CompilationState& state, const zasm_script* script, const CmpGroup& g, bool cmp_bool)
{
	WasmAssembler& wasm = *state.wasm;
	if (g.is_strcmp)
	{
		// check_cmp reads a string comparison as its result against 0 (a
		// bool-cast string comparison is never emitted).
		wasm.emitLocalGet(l_idx_cmp1);
		wasm.emitI32Const(0);
		return;
	}
	auto push_operand = [&](int arg, bool is_imm, uint8_t local){
		if (is_imm)
		{
			wasm.emitI32Const(cmp_bool ? !!arg : arg);
			return;
		}
		if (g.capture)
			wasm.emitLocalGet(local);
		else
			get_z_register(state, arg);
		if (cmp_bool)
		{
			wasm.emitI32Const(0);
			wasm.emitI32Ne();
		}
	};
	push_operand(script->zasm[g.cmp_pc].arg1, g.arg1_is_imm, l_idx_cmp1);
	push_operand(script->zasm[g.cmp_pc].arg2, g.arg2_is_imm, l_idx_cmp2);
}

// Consumes the two operands, leaving the boolean the CMP_FLAGS select.
static void emit_cmp_flags_op(WasmAssembler& wasm, int flags)
{
	switch (flags & CMP_FLAGS)
	{
		default: wasm.emitDrop(); wasm.emitDrop(); wasm.emitI32Const(0); break;
		case CMP_GT: wasm.emitI32GtS(); break;
		case CMP_GT|CMP_EQ: wasm.emitI32GeS(); break;
		case CMP_LT: wasm.emitI32LtS(); break;
		case CMP_LT|CMP_EQ: wasm.emitI32LeS(); break;
		case CMP_EQ: wasm.emitI32Eq(); break;
		case CMP_GT|CMP_LT: wasm.emitI32Ne(); break;
		case CMP_GT|CMP_LT|CMP_EQ: wasm.emitDrop(); wasm.emitDrop(); wasm.emitI32Const(1); break;
	}
}

// Emits one SETx consumer at cj.
static void emit_cmp_set_consumer(CompilationState& state, const zasm_script* script, const CmpGroup& g, pc_t cj)
{
	WasmAssembler& wasm = *state.wasm;
	int cc = script->zasm[cj].command;
	int c_arg1 = script->zasm[cj].arg1;
	int c_arg2 = script->zasm[cj].arg2;
	bool cmp_bool = c_arg2 & CMP_BOOL;

	if (cc == SETCMP)
	{
		set_z_register(state, c_arg1, [&](){
			emit_cmp_operands(state, script, g, cmp_bool);
			emit_cmp_flags_op(wasm, c_arg2);
			if (c_arg2 & CMP_SETI) { wasm.emitI32Const(10000); wasm.emitI32Mul(); }
		});
	}
	else if (cc == SETLESSI || cc == SETLESS)
	{
		set_z_register(state, c_arg1, [&](){
			emit_cmp_operands(state, script, g, cmp_bool); wasm.emitI32LeS();
			if (cc == SETLESSI) { wasm.emitI32Const(10000); wasm.emitI32Mul(); }
		});
	}
	else if (cc == SETMOREI || cc == SETMORE)
	{
		set_z_register(state, c_arg1, [&](){
			emit_cmp_operands(state, script, g, cmp_bool); wasm.emitI32GeS();
			if (cc == SETMOREI) { wasm.emitI32Const(10000); wasm.emitI32Mul(); }
		});
	}
	else if (cc == SETFALSEI || cc == SETFALSE)
	{
		set_z_register(state, c_arg1, [&](){
			emit_cmp_operands(state, script, g, cmp_bool); wasm.emitI32Ne();
			if (cc == SETFALSEI) { wasm.emitI32Const(10000); wasm.emitI32Mul(); }
		});
	}
	else if (cc == SETTRUEI || cc == SETTRUE)
	{
		set_z_register(state, c_arg1, [&](){
			emit_cmp_operands(state, script, g, cmp_bool); wasm.emitI32Eq();
			if (cc == SETTRUEI) { wasm.emitI32Const(10000); wasm.emitI32Mul(); }
		});
	}
	else
	{
		error(script, fmt::format("unexpected comparison consumer {} at index {}", zasm_op_to_string(cc).c_str(), (int)cj));
		CHECK(false);
	}
}

// Emits the take-branch value for one GOTOx consumer at cj: leaves an i32 on
// the stack, nonzero meaning the branch to the goto target is taken.
static void emit_cmp_goto_value(CompilationState& state, const zasm_script* script, const CmpGroup& g, pc_t cj)
{
	WasmAssembler& wasm = *state.wasm;
	int cc = script->zasm[cj].command;
	int c_arg2 = script->zasm[cj].arg2;
	bool cmp_bool = c_arg2 & CMP_BOOL;

	emit_cmp_operands(state, script, g, cmp_bool);
	switch (cc)
	{
		case GOTOCMP: emit_cmp_flags_op(wasm, c_arg2); break;
		case GOTOTRUE: wasm.emitI32Eq(); break;
		case GOTOFALSE: wasm.emitI32Ne(); break;
		case GOTOMORE: wasm.emitI32GeS(); break;
		case GOTOLESS:
			// `<=` unless the compat rule makes GOTOLESS strict, like the interpreter.
			if (get_qr(qr_GOTOLESSNOTEQUAL)) wasm.emitI32LtS(); else wasm.emitI32LeS();
			break;
	}
}

// Compiles a comparison command at `i` together with every consumer, for the
// loop-switch lowering. Returns the pc of the last command consumed (the
// caller's loop advances past it); for a loose compare with no consumer,
// returns `i`. num_frames/current_rank/current_block_index are the caller's
// scaffold trackers (see compile_function); a consumer run may cross block
// boundaries, which closes scaffold frames.
static pc_t compile_comparison(CompilationState& state, const zasm_script* script, const ZasmCFG& cfg, int num_frames, int& current_rank, pc_t& current_block_index, pc_t i)
{
	WasmAssembler& wasm = *state.wasm;

	CmpGroup g = analyze_comparison(script, i);
	if (g.consumers.empty())
	{
		// A loose compare (e.g. the statement `x == 1;`). Ignore it.
		return i;
	}

	emit_cmp_capture(state, script, g);

	for (pc_t cj : g.consumers)
	{
		// O(1) equivalent of cfg.contains_block_start(cj): during block b's body
		// current_block_index == b + 1 (see compile_function).
		if (current_block_index < cfg.block_starts.size() && cfg.block_starts[current_block_index] == cj)
		{
			// Consumer blocks are never absorbed into a structured region
			// (detect_yielder_regions drops any region a run leaks into), so
			// every one of these has a scaffold frame.
			wasm.emitEnd();
			current_block_index += 1;
			current_rank += 1;
		}

		int cc = script->zasm[cj].command;
		if (cc == NOP)
		{
			// The optimizer can turn a consumer into a NOP without removing
			// the compare; nothing to emit.
			//
			// Example: hollow_forest.qst/zasm-global-1.txt
			// 11911: COMPAREV        D2              0            
			// 11912: GOTOTRUE        11913                        ---- turns into NOP
			continue;
		}

		if (command_is_goto_consumer(cc))
		{
			emit_cmp_goto_value(state, script, g, cj);

			int target_pc = script->zasm[cj].arg1;
			if (target_pc < (int)state.start_pc || target_pc > (int)state.final_pc)
			{
				wasm.emitIf();
				emit_exit_continue(state, target_pc);
				wasm.emitEnd();
			}
			else
			{
				size_t target_block_index = cfg.block_id_from_start_pc(target_pc) + 1;
				wasm.emitI32Const(target_block_index);
				wasm.emitLocalSet(l_idx_target_block);
				wasm.emitBrIf(num_frames - current_rank);
			}
		}
		else
		{
			emit_cmp_set_consumer(state, script, g, cj);
		}
	}

	return g.consumers.back();
}

// Whether a call to target_pc completes before control comes back to the caller - so the caller
// can call it directly, and needs no resume point after the call: the callee is a function (not
// some pc in the middle of one, which constructor delegation in older scripts calls) that can
// never yield.
static bool is_direct_callable(const StructuredZasm& structured_zasm, int target_pc)
{
	auto it = structured_zasm.start_pc_to_function.find(target_pc);
	return it != structured_zasm.start_pc_to_function.end() && !structured_zasm.functions[it->second].may_yield;
}

// A call through the driver, which pushes the return pc and enters the callee; this piece is
// re-entered after the CALLFUNC once the callee returns (see pc_to_resume_address).
static void compile_callfunc_driver(CompilationState& state, pc_t i, int target_pc)
{
	emit_store_ctx_const(state, offsetof(JittedExecutionContext, pc), i);
	emit_store_ctx_const(state, offsetof(JittedExecutionContext, call_pc), target_pc);
	emit_exit(state, EXEC_RESULT_CALL);
}

// A call to a function that can't yield, made directly through its slot in direct_entry_table
// (which holds jit_wasm_direct_not_compiled until it is compiled, running the call through the
// driver to its end). A normal return continues after the call; any other exec result (the script
// stopping, or something unusual like a call to a pc inside some function) leaves this function
// too, and if the driver comes back here it is with no resume point - the interpreter runs the
// rest of the function.
static void compile_callfunc_direct(CompilationState& state, pc_t i, int target_pc)
{
	auto& wasm = *state.wasm;
	const auto& structured_zasm = state.j_script->structured_zasm;
	pc_t fn_id = structured_zasm.start_pc_to_function.at(target_pc);

	emit_get_ri(state);
	wasm.emitI32Load(offsetof(refInfo, retsp));
	wasm.emitI32Const(max_direct_call_depth);
	wasm.emitI32GeU();
	wasm.emitIf();
	compile_callfunc_driver(state, i, target_pc);
	wasm.emitEnd();

	emit_store_ctx(state, offsetof(JittedExecutionContext, sp), [&]{ emit_get_sp(state); });
	emit_store_ctx_const(state, offsetof(JittedExecutionContext, call_pc), i + 1);
	emit_store_ctx_const(state, offsetof(JittedExecutionContext, entry_mode), 1);
	wasm.emitLocalGet(l_idx_ctx);
	wasm.emitI32Const((int32_t)(uintptr_t)&state.j_script->direct_entry_table[fn_id]);
	wasm.emitI32Load(0);
	wasm.emitCallIndirect(state.exec_type);

	wasm.emitLocalTee(l_idx_scratch);
	wasm.emitI32Const(EXEC_RESULT_RETURN);
	wasm.emitI32Ne();
	wasm.emitIf();
	wasm.emitLocalGet(l_idx_scratch);
	wasm.emitReturn();
	wasm.emitEnd();

	// The callee consumed its arguments from the stack.
	emit_load_ctx(state, offsetof(JittedExecutionContext, sp));
	emit_set_sp(state);
	// As the driver does on every return (error traces read it).
	emit_store_ctx_const(state, offsetof(JittedExecutionContext, pc), i + 1);
}

static void compile_callfunc(CompilationState& state, pc_t i, int target_pc)
{
	if (is_direct_callable(state.j_script->structured_zasm, target_pc))
		compile_callfunc_direct(state, i, target_pc);
	else
		compile_callfunc_driver(state, i, target_pc);
}

// A GOTO to outside this piece - the entry of some other function (only expected for the "~Init"
// script of older, 2.55-era quests, which is assembled by tail-jumping over inlined init function
// bodies to the "run" continuation, rather than using a CALLFUNC). The driver continues there.
//
// Example: .tmp/replay_uploads/EB5E2CFAE26A97BAA15637C0A60D557A/6940ead2-143f-45b9-a894-1bc05c81b9e0-updated-main.zplay
static void compile_goto_out_of_piece(CompilationState& state, int target_pc)
{
	emit_exit_continue(state, target_pc);
}

static void compile_quit(CompilationState& state, const zasm_script* script, pc_t i)
{
	compile_command_interpreter(state, script, i, 1);
	emit_exit_stop(state, RUNSCRIPT_STOPPED, i);
}

// Emits a GOTORANGES binary search (see the opcode's comment in defines.h).
// `branch(target_index, extra)` emits the transfer for range target_index (or
// the default, when target_index is -1), where `extra` is how many wasm frames
// this search has open at that point - the caller adds it to its own branch
// depth. The search only narrows to the range a matching key would be in, so
// each leaf still checks its own bounds and falls back to the default.
template <typename Branch>
static void emit_gotoranges_tree(CompilationState& state, const zasm_script* script, pc_t pc, Branch branch)
{
	WasmAssembler& wasm = *state.wasm;
	const auto& table = *script->literals.vec(script->zasm[pc]);
	size_t num_ranges = gotoranges_count(table);

	if (num_ranges == 0)
	{
		branch(-1, 0);
		return;
	}

	get_z_register(state, script->zasm[pc].arg1);
	wasm.emitLocalSet(l_idx_scratch2);

	auto emit_tree = [&](auto&& self, size_t lo, size_t hi, int extra) -> void {
		if (lo == hi)
		{
			auto range = gotoranges_at(table, lo);
			wasm.emitLocalGet(l_idx_scratch2);
			wasm.emitI32Const(range.start);
			wasm.emitI32GeS();
			wasm.emitLocalGet(l_idx_scratch2);
			wasm.emitI32Const(range.end);
			wasm.emitI32LeS();
			wasm.emitI32And();
			wasm.emitIf();
			branch((int)lo, extra + 1);
			wasm.emitEnd();
			branch(-1, extra);
			return;
		}
		size_t mid = (lo + hi + 1) / 2;
		wasm.emitLocalGet(l_idx_scratch2);
		wasm.emitI32Const(gotoranges_at(table, mid).start);
		wasm.emitI32GeS();
		wasm.emitIf();
		self(self, mid, hi, extra + 1);
		wasm.emitEnd();
		self(self, lo, mid - 1, extra);
	};
	emit_tree(emit_tree, 0, num_ranges - 1, 0);
}

// The structured lowering's range search: branch out to structurer-provided
// depths, adjusted by the frames the search itself opens.
static void emit_gotoranges_search(CompilationState& state, const zasm_script* script, pc_t pc,
	const std::vector<int>& depths, int default_depth)
{
	emit_gotoranges_tree(state, script, pc, [&](int target_index, int extra) {
		int depth = target_index < 0 ? default_depth : depths[target_index];
		state.wasm->emitBr(depth + extra);
	});
}

// Pushes a GOTOTABLE's dense table index as a u32 (see the opcode's comment in
// defines.h): (key - min_key)/10000 when the key is an exact multiple, else -1.
// Any out-of-range value - including the -1, and negative quotients read as
// huge u32s - selects a br_table's default entry, so callers need no separate
// bounds check when this feeds a br_table.
static void emit_gototable_index(CompilationState& state, const zasm_script* script, pc_t pc)
{
	WasmAssembler& wasm = *state.wasm;
	const auto& table = *script->literals.vec(script->zasm[pc]);

	get_z_register(state, script->zasm[pc].arg1);
	wasm.emitI32Const(table[0]);
	wasm.emitI32Sub();
	wasm.emitLocalTee(l_idx_scratch);
	wasm.emitI32Const(10000);
	wasm.emitI32DivS();
	wasm.emitI32Const(-1);
	wasm.emitLocalGet(l_idx_scratch);
	wasm.emitI32Const(10000);
	wasm.emitI32RemS();
	wasm.emitI32Eqz();
	wasm.emitSelect(); // exact multiple ? quotient : -1
}

// Emits one straight-line ("plain") compiled command - everything except
// control flow, which each lowering (the loop-switch in compile_function, or
// the structured path in compile_function_structured) handles itself.
static void compile_plain_command(CompilationState& state, const zasm_script* script, pc_t i)
{
	WasmAssembler& wasm = *state.wasm;

	int command = script->zasm[i].command;
	int arg1 = script->zasm[i].arg1;
	int arg2 = script->zasm[i].arg2;

	// Every command here must be reflected in command_is_compiled!
	switch (command)
	{
		case NOP: break;

		case PUSHR:
		case PUSHV:
		{
			// Evaluate the value to push BEFORE decrementing sp, matching the
			// interpreter and x64 JIT. A stack-dependent register (e.g.
			// SCREENDATAEXDOOR) read after add_sp would see the decremented sp
			// and read the wrong stack slot. Capturing into a local also handles
			// SP/SP2 (which read the current sp) without a special case.
			if (command == PUSHR)
				get_z_register(state, arg1);
			else
				wasm.emitI32Const(arg1);
			wasm.emitLocalSet(l_idx_scratch);

			add_sp(state, -1);
			check_sp(state);

			emit_get_sp(state);
			wasm.emitI32Const(4);
			wasm.emitI32Mul(); // Multiply by 4 to get byte offset.
			emit_get_stack_base(state);
			wasm.emitI32Add(); // Add stack base offset.
			wasm.emitLocalGet(l_idx_scratch);
			wasm.emitI32Store();
		}
		break;

		case PUSHARGSR:
		case PUSHARGSV:
		{
			if (command == PUSHARGSR)
			{
				get_z_register(state, arg1);
				wasm.emitLocalSet(l_idx_scratch);
			}

			// TODO: there's certainly a better way to do this.
			for (int i = 0; i < arg2; i++)
			{
				add_sp(state, -1);
				check_sp(state);

				emit_get_sp(state);
				wasm.emitI32Const(4);
				wasm.emitI32Mul(); // Multiply by 4 to get byte offset.
				emit_get_stack_base(state);
				wasm.emitI32Add(); // Add stack base offset.
				if (command == PUSHARGSR)
					wasm.emitLocalGet(l_idx_scratch);
				else
					wasm.emitI32Const(arg1);
				wasm.emitI32Store();
			}
		}
		break;

		case SETR:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg2);
			});
		}
		break;
		case SETV:
		{
			// For test_jit_runtime_debug_test.
			static bool jit_runtime_debug_test_force_bug = get_flag_bool("-jit-runtime-debug-test-force-bug").value_or(false);
			if (jit_runtime_debug_test_force_bug) arg2++;

			set_z_register(state, arg1, [&](){
				wasm.emitI32Const(arg2);
			});
		}
		break;
		case STACKWRITEATVV:
		{
			// Lit[arg1] -> Stack[arg2]
			emit_get_sp(state);
			wasm.emitI32Const(arg2);
			wasm.emitI32Add();
			wasm.emitI32Const(4);
			wasm.emitI32Mul(); // Multiply by 4 to get byte offset.
			emit_get_stack_base(state);
			wasm.emitI32Add();
			wasm.emitI32Const(arg1);
			wasm.emitI32Store();
		}
		break;
		case STORE:
		{
			// Reg[arg1] -> Stack[rSFRAME + arg2]
			get_z_register(state, rSFRAME);
			if (arg2)
			{
				wasm.emitI32Const(arg2);
				wasm.emitI32Add();
			}

			wasm.emitI32Const(4);
			wasm.emitI32Mul(); // Multiply by 4 to get byte offset.
			emit_get_stack_base(state);
			wasm.emitI32Add();
			get_z_register(state, arg1);
			wasm.emitI32Store();
		}
		break;
		case STORED:
		{
			// Reg[arg1] -> Stack[rSFRAME + arg2]
			get_z_register(state, rSFRAME);
			wasm.emitI32Const(10000);
			wasm.emitI32DivU();
			if (arg2)
			{
				wasm.emitI32Const(arg2 / 10000);
				wasm.emitI32Add();
			}

			wasm.emitI32Const(4);
			wasm.emitI32Mul(); // Multiply by 4 to get byte offset.
			emit_get_stack_base(state);
			wasm.emitI32Add();
			get_z_register(state, arg1);
			wasm.emitI32Store();
		}
		break;
		case LOAD:
		{
			// Stack[rSFRAME + arg2] -> Reg[arg1]
			set_z_register(state, arg1, [&](){
				get_z_register(state, rSFRAME);
				if (arg2)
				{
					wasm.emitI32Const(arg2);
					wasm.emitI32Add();
				}
				wasm.emitI32Const(4);
				wasm.emitI32Mul(); // Multiply by 4 to get byte offset.
				emit_get_stack_base(state);
				wasm.emitI32Add();
				wasm.emitI32Load();
			});
		}
		break;
		case LOADD:
		{
			// Stack[rSFRAME + arg2] -> Reg[arg1]
			set_z_register(state, arg1, [&](){
				get_z_register(state, rSFRAME);
				wasm.emitI32Const(10000);
				wasm.emitI32DivU();
				if (arg2)
				{
					wasm.emitI32Const(arg2 / 10000);
					wasm.emitI32Add();
				}

				wasm.emitI32Const(4);
				wasm.emitI32Mul(); // Multiply by 4 to get byte offset.
				emit_get_stack_base(state);
				wasm.emitI32Add();
				wasm.emitI32Load();
			});
		}
		break;
		case POP:
		{
			set_z_register(state, arg1, [&](){
				emit_get_sp(state);
				wasm.emitI32Const(4);
				wasm.emitI32Mul(); // Multiply by 4 to get byte offset.
				emit_get_stack_base(state);
				wasm.emitI32Add(); // Add stack base offset.
				wasm.emitI32Load();

				add_sp(state, 1);
			});
		}
		break;

		case POPARGS:
		{
			add_sp(state, arg2);

			set_z_register(state, arg1, [&](){
				// ri->sp - 1
				emit_get_sp(state);
				wasm.emitI32Const(1);
				wasm.emitI32Sub();

				wasm.emitI32Const(4);
				wasm.emitI32Mul();

				emit_get_stack_base(state);
				wasm.emitI32Add(); // Add stack base offset.
				wasm.emitI32Load();
			});
		}
		break;

		case DIVR:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg2);
				wasm.emitLocalTee(l_idx_scratch);

				wasm.emitIf(WasmSimpleBlockType::I32);
				get_z_register(state, arg1);
				wasm.emitI32ExtendS();
				wasm.emitI64Const(10000);
				wasm.emitI64Mul();
				wasm.emitLocalGet(l_idx_scratch);
				wasm.emitI32ExtendS();
				wasm.emitI64DivS();
				wasm.emitI64Wrap();

				wasm.emitElse();
				wasm.emitI32Const(0);
				wasm.emitCall(state.f_idx_log_error);
				// sign(dividend) * MAX_SIGNED_32, like the interpreter (whose sign(0) is 1).
				get_z_register(state, arg1);
				wasm.emitI32Const(31);
				wasm.emitI32ShrS();
				wasm.emitI32Const(1);
				wasm.emitI32Or();
				wasm.emitI32Const(MAX_SIGNED_32);
				wasm.emitI32Mul();
				wasm.emitEnd();
			});
		}
		break;

		case PEEK:
		{
			set_z_register(state, arg1, [&](){
				emit_get_sp(state);
				wasm.emitI32Const(4);
				wasm.emitI32Mul(); // Multiply by 4 to get byte offset.
				emit_get_stack_base(state);
				wasm.emitI32Add(); // Add stack base offset.
				wasm.emitI32Load();
			});
		}
		break;

		case DIVV:
		{
			set_z_register(state, arg1, [&](){
				if (arg2 == 0)
				{
					wasm.emitI32Const(0);
					wasm.emitCall(state.f_idx_log_error);
					// sign(dividend) * MAX_SIGNED_32, like the interpreter (whose sign(0) is 1).
					get_z_register(state, arg1);
					wasm.emitI32Const(31);
					wasm.emitI32ShrS();
					wasm.emitI32Const(1);
					wasm.emitI32Or();
					wasm.emitI32Const(MAX_SIGNED_32);
					wasm.emitI32Mul();
					return;
				}

				get_z_register(state, arg1);
				wasm.emitI32ExtendS();
				wasm.emitI64Const(10000);
				wasm.emitI64Mul();
				wasm.emitI64Const(arg2);
				wasm.emitI64DivS();
				wasm.emitI64Wrap();
			});
		}
		break;

		case ADDR:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				get_z_register(state, arg2);
				wasm.emitI32Add();
			});
		}
		break;
		case ADDV:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(arg2);
				wasm.emitI32Add();
			});
		}
		break;

		case CASTBOOLF:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(0);
				wasm.emitI32Ne();
			});
		}
		break;
		case CASTBOOLI:
		{
			set_z_register(state, arg1, [&](){
				wasm.emitI32Const(10000);
				wasm.emitI32Const(0);
				get_z_register(state, arg1);
				wasm.emitSelect();
			});
		}
		break;

		case MODR:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg2);
				wasm.emitLocalTee(l_idx_scratch);

				wasm.emitIf(WasmSimpleBlockType::I32);
				get_z_register(state, arg1);
				wasm.emitLocalGet(l_idx_scratch);
				wasm.emitI32RemS();

				wasm.emitElse();
				wasm.emitI32Const(1);
				wasm.emitCall(state.f_idx_log_error);
				wasm.emitI32Const(0);
				wasm.emitEnd();
			});
		}
		break;
		case MODV:
		{
			set_z_register(state, arg1, [&](){
				if (arg2 == 0)
				{
					wasm.emitI32Const(1);
					wasm.emitCall(state.f_idx_log_error);
					wasm.emitI32Const(0);
					return;
				}

				get_z_register(state, arg1);
				wasm.emitI32Const(arg2);
				wasm.emitI32RemS();
			});
		}
		break;

		case MULTR:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32ExtendS();
				get_z_register(state, arg2);
				wasm.emitI32ExtendS();
				wasm.emitI64Mul();
				wasm.emitI64Const(10000);
				wasm.emitI64DivS();
				wasm.emitI64Wrap();
			});
		}
		break;
		case MULTV:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32ExtendS();
				wasm.emitI64Const(arg2);
				wasm.emitI64Mul();
				wasm.emitI64Const(10000);
				wasm.emitI64DivS();
				wasm.emitI64Wrap();
			});
		}
		break;

		case SUBR:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				get_z_register(state, arg2);
				wasm.emitI32Sub();
			});
		}
		break;
		case SUBV:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(arg2);
				wasm.emitI32Sub();
			});
		}
		break;
		case SUBV2:
		{
			// reg = arg1 - reg (note: the destination is arg2).
			set_z_register(state, arg2, [&](){
				wasm.emitI32Const(arg1);
				get_z_register(state, arg2);
				wasm.emitI32Sub();
			});
		}
		break;

		case ABS:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitLocalSet(l_idx_scratch);
				// val < 0 ? -val : val
				wasm.emitI32Const(0);
				wasm.emitLocalGet(l_idx_scratch);
				wasm.emitI32Sub();
				wasm.emitLocalGet(l_idx_scratch);
				wasm.emitLocalGet(l_idx_scratch);
				wasm.emitI32Const(0);
				wasm.emitI32LtS();
				wasm.emitSelect();
			});
		}
		break;

		case MINR:
		case MINV:
		case MAXR:
		case MAXV:
		{
			bool is_min = command == MINR || command == MINV;
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitLocalSet(l_idx_scratch);
				if (command == MINR || command == MAXR)
					get_z_register(state, arg2);
				else
					wasm.emitI32Const(arg2);
				wasm.emitLocalSet(l_idx_scratch2);

				wasm.emitLocalGet(l_idx_scratch);
				wasm.emitLocalGet(l_idx_scratch2);
				wasm.emitLocalGet(l_idx_scratch);
				wasm.emitLocalGet(l_idx_scratch2);
				if (is_min)
					wasm.emitI32LtS();
				else
					wasm.emitI32GtS();
				wasm.emitSelect();
			});
		}
		break;

		// AND/OR operate on the integer part: (a/10000 OP b/10000) * 10000.
		case ANDR:
		case ANDV:
		case ORR:
		case ORV:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(10000);
				wasm.emitI32DivS();
				if (command == ANDR || command == ORR)
				{
					get_z_register(state, arg2);
					wasm.emitI32Const(10000);
					wasm.emitI32DivS();
				}
				else
					wasm.emitI32Const(arg2 / 10000);
				if (command == ANDR || command == ANDV)
					wasm.emitI32And();
				else
					wasm.emitI32Or();
				wasm.emitI32Const(10000);
				wasm.emitI32Mul();
			});
		}
		break;
		case ORR32:
		case ORV32:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				if (command == ORR32)
					get_z_register(state, arg2);
				else
					wasm.emitI32Const(arg2);
				wasm.emitI32Or();
			});
		}
		break;

		// FLOOR/CEILING round the fixed-point value to a whole number
		// (a multiple of 10000), toward -/+ infinity. Matches zfix
		// getFloor/getCeil: q = val/10000 (truncating), then adjust q by
		// one when there is a remainder in the rounding direction.
		case FLOOR:
		case CEILING:
		{
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitLocalTee(l_idx_scratch);
				wasm.emitI32Const(10000);
				wasm.emitI32DivS();
				// (val % 10000 != 0) & (val < 0 for floor / val > 0 for ceil)
				wasm.emitLocalGet(l_idx_scratch);
				wasm.emitI32Const(10000);
				wasm.emitI32RemS();
				wasm.emitI32Const(0);
				wasm.emitI32Ne();
				wasm.emitLocalGet(l_idx_scratch);
				wasm.emitI32Const(0);
				if (command == FLOOR)
					wasm.emitI32LtS();
				else
					wasm.emitI32GtS();
				wasm.emitI32And();
				if (command == FLOOR)
					wasm.emitI32Sub();
				else
					wasm.emitI32Add();
				wasm.emitI32Const(10000);
				wasm.emitI32Mul();
			});
		}
		break;

		case LOADI:
		{
			// Stack[reg[arg2] / 10000] -> Reg[arg1]
			// Like the x64 JIT (but unlike the interpreter), no stack
			// bounds check - same as the LOAD/STORE family above.
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg2);
				wasm.emitI32Const(10000);
				wasm.emitI32DivS();
				wasm.emitI32Const(4);
				wasm.emitI32Mul(); // Multiply by 4 to get byte offset.
				emit_get_stack_base(state);
				wasm.emitI32Add();
				wasm.emitI32Load();
			});
		}
		break;
		case STOREI:
		{
			// Reg[arg1] -> Stack[reg[arg2] / 10000]
			get_z_register(state, arg2);
			wasm.emitI32Const(10000);
			wasm.emitI32DivS();
			wasm.emitI32Const(4);
			wasm.emitI32Mul(); // Multiply by 4 to get byte offset.
			emit_get_stack_base(state);
			wasm.emitI32Add();
			get_z_register(state, arg1);
			wasm.emitI32Store();
		}
		break;

		case BITNOT:
		{
			// reg = (~(reg / 10000)) * 10000  (~x == x ^ -1)
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(10000);
				wasm.emitI32DivS();
				wasm.emitI32Const(-1);
				wasm.emitI32Xor();
				wasm.emitI32Const(10000);
				wasm.emitI32Mul();
			});
		}
		break;
		case BITNOT32:
		{
			// reg = ~reg
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(-1);
				wasm.emitI32Xor();
			});
		}
		break;
		case LSHIFTV:
		case RSHIFTV:
		{
			// reg = ((reg / 10000) <</>> k) * 10000, k is a constant count.
			// wasm masks the shift count to 5 bits, matching the interpreter
			// (whose C++ shifts also lower to masked shifts); >> is arithmetic.
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(10000);
				wasm.emitI32DivS();
				wasm.emitI32Const(arg2 / 10000);
				if (command == LSHIFTV) wasm.emitI32Shl();
				else wasm.emitI32ShrS();
				wasm.emitI32Const(10000);
				wasm.emitI32Mul();
			});
		}
		break;
		case LSHIFTV32:
		case RSHIFTV32:
		{
			// reg = reg <</>> k (raw, no fixed-point scaling on the value).
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(arg2 / 10000);
				if (command == LSHIFTV32) wasm.emitI32Shl();
				else wasm.emitI32ShrS();
			});
		}
		break;
		case LSHIFTR:
		case RSHIFTR:
		{
			// reg = ((reg / 10000) <</>> (count / 10000)) * 10000, count is a register.
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(10000);
				wasm.emitI32DivS();
				get_z_register(state, arg2);
				wasm.emitI32Const(10000);
				wasm.emitI32DivS();
				if (command == LSHIFTR) wasm.emitI32Shl();
				else wasm.emitI32ShrS();
				wasm.emitI32Const(10000);
				wasm.emitI32Mul();
			});
		}
		break;
		case LSHIFTR32:
		case RSHIFTR32:
		{
			// reg = reg <</>> (count / 10000) (raw value, count is a register).
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				get_z_register(state, arg2);
				wasm.emitI32Const(10000);
				wasm.emitI32DivS();
				if (command == LSHIFTR32) wasm.emitI32Shl();
				else wasm.emitI32ShrS();
			});
		}
		break;
		case XORV:
		{
			// reg = ((reg / 10000) ^ (arg2 / 10000)) * 10000
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(10000);
				wasm.emitI32DivS();
				wasm.emitI32Const(arg2 / 10000);
				wasm.emitI32Xor();
				wasm.emitI32Const(10000);
				wasm.emitI32Mul();
			});
		}
		break;
		case XORR:
		{
			// reg = ((reg / 10000) ^ (arg2reg / 10000)) * 10000
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(10000);
				wasm.emitI32DivS();
				get_z_register(state, arg2);
				wasm.emitI32Const(10000);
				wasm.emitI32DivS();
				wasm.emitI32Xor();
				wasm.emitI32Const(10000);
				wasm.emitI32Mul();
			});
		}
		break;
		case XORV32:
		{
			// reg = reg ^ arg2 (raw)
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(arg2);
				wasm.emitI32Xor();
			});
		}
		break;
		case XORR32:
		{
			// reg = reg ^ arg2reg (raw)
			set_z_register(state, arg1, [&](){
				get_z_register(state, arg1);
				get_z_register(state, arg2);
				wasm.emitI32Xor();
			});
		}
		break;

		case READPODARRAYR:
		case READPODARRAYV:
		{
			// reg[arg1] = array[rINDEX][index]. Calls a helper for the bounds-checked
			// access (array pointer + index passed as args), so no register flush.
			set_z_register(state, arg1, [&](){
				get_z_register(state, rINDEX);
				if (command == READPODARRAYR)
				{
					get_z_register(state, arg2);
					wasm.emitI32Const(10000);
					wasm.emitI32DivS();
				}
				else
				{
					wasm.emitI32Const(arg2 / 10000);
				}
				wasm.emitI32Const(state.pc);
				wasm.emitI32Const(0); // no_neg: honor the negative-index QR
				wasm.emitCall(state.f_idx_pod_read);
			});
		}
		break;
		case WRITEPODARRAYRR:
		case WRITEPODARRAYRV:
		case WRITEPODARRAYVR:
		case WRITEPODARRAYVV:
		{
			// array[rINDEX][index] = value. First suffix letter is the index operand
			// kind, second is the value operand kind (R=register, V=immediate).
			bool index_is_reg = command == WRITEPODARRAYRR || command == WRITEPODARRAYRV;
			bool value_is_reg = command == WRITEPODARRAYRR || command == WRITEPODARRAYVR;

			get_z_register(state, rINDEX);
			if (index_is_reg)
			{
				get_z_register(state, arg1);
				wasm.emitI32Const(10000);
				wasm.emitI32DivS();
			}
			else
			{
				wasm.emitI32Const(arg1 / 10000);
			}
			if (value_is_reg)
				get_z_register(state, arg2);
			else
				wasm.emitI32Const(arg2);
			wasm.emitI32Const(script->zasm[i].arg3);
			wasm.emitI32Const(state.pc);
			wasm.emitI32Const(0); // no_neg: honor the negative-index QR
			wasm.emitCall(state.f_idx_pod_write);
		}
		break;
		case WRITEPODARRAY:
		{
			// Bulk-initialize array (reg arg1) from the instruction's constant vector.
			get_z_register(state, arg1);
			wasm.emitI32Const(state.pc);
			wasm.emitCall(state.f_idx_writepodarr);
		}
		break;
		case ALLOCATEMEMV:
		{
			// reg[arg1] = allocate(size=arg2/10000, object_type=arg3).
			set_z_register(state, arg1, [&](){
				wasm.emitI32Const(arg2 / 10000);
				wasm.emitI32Const(script->zasm[i].arg3);
				wasm.emitI32Const(state.pc);
				wasm.emitCall(state.f_idx_allocatemem);
			});
		}
		break;
		case ZCLASS_READ:
		{
			// rEXP1 = object(arg1).data[arg2]. The helper does the id and bounds checks.
			set_z_register(state, rEXP1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(arg2);
				wasm.emitI32Const(state.pc);
				wasm.emitCall(state.f_idx_class_read);
			});
		}
		break;
		case ZCLASS_WRITE:
		{
			// object(arg1).data[arg2] = rEXP1. The helper also handles reference counts
			// for object-typed members.
			get_z_register(state, arg1);
			wasm.emitI32Const(arg2);
			get_z_register(state, rEXP1);
			wasm.emitI32Const(state.pc);
			wasm.emitCall(state.f_idx_class_write);
		}
		break;
		case REF_INC:
		case REF_DEC:
		{
			// arg1 is the object's stack offset from rSFRAME, or -1 for `this`.
			bool is_this = arg1 == -1;
			wasm.emitI32Const(is_this ? 1 : 0);
			if (is_this)
			{
				wasm.emitI32Const(0);
			}
			else
			{
				get_z_register(state, rSFRAME);
				if (arg1)
				{
					wasm.emitI32Const(arg1);
					wasm.emitI32Add();
				}
			}
			wasm.emitI32Const(command == REF_INC ? 1 : 0);
			wasm.emitI32Const(state.pc);
			wasm.emitCall(state.f_idx_ref_change);
		}
		break;
		case MARK_TYPE_STACK:
		{
			get_z_register(state, rSFRAME);
			if (arg2)
			{
				wasm.emitI32Const(arg2);
				wasm.emitI32Add();
			}
			wasm.emitI32Const(arg1 ? 1 : 0);
			wasm.emitCall(state.f_idx_mark_type_stack);
		}
		break;
		case ARRAYSIZE:
		{
			// rEXP1 = size of the array in arg1.
			set_z_register(state, rEXP1, [&](){
				get_z_register(state, arg1);
				wasm.emitI32Const(state.pc);
				wasm.emitCall(state.f_idx_arraysize);
			});
		}
		break;

		default:
		{
			printf("unexpected command %s at index %d\n", zasm_op_to_string(command).c_str(), (int)i);
			CHECK(false);
		}
	}
}

// Structured ("relooped") compilation, for non-yielding functions.
//
// Instead of the loop-switch dispatch (see compile_function below), the
// function's CFG is reconstructed into real wasm block/loop/if control flow
// via WasmStructurer (Ramsey, "Beyond Relooper", ICFP 2022). Script loops
// become real wasm loops - no global write + br_table dispatch per iteration,
// and the browser's optimizing tier can see the loop nest.
//
// Only non-yielding functions qualify: the yielder's call/return/resume
// targets are runtime values (a computed goto), which structured control flow
// cannot express, so it stays on the loop-switch.
//
// compile_function_structured returns nullopt when the function can't be
// structured - an irreducible CFG, or a control shape the classifier doesn't
// model (e.g. a comparison-goto with no reaching compare) - and the caller
// falls back to the loop-switch lowering.

static void emit_function_end(CompilationState& state, const StructuredZasm& structured_zasm, const ZasmFunction& fn, bool end_function = true);

struct StructuredFnSink : StructSink
{
	CompilationState* state;
	const zasm_script* script;
	const StructuredZasm* structured_zasm;
	const std::vector<pc_t>* starts;
	// Last pc each block's body emits; the block's terminating branch (if any)
	// is emitted by the structurer via BlockInfo / emit_cond instead.
	const std::vector<int>* body_last;
	// For a block terminated by an intra-function GOTO (emitted by the
	// structurer as a bare branch), the GOTO's pc - so its debug marker and
	// runtime_debug call still appear, matching the loop-switch lowering and
	// the interpreter's trace. -1 otherwise.
	const std::vector<int>* goto_debug_pc;
	// For a Term::Switch block, the GOTOTABLE's pc; -1 otherwise. Null for the
	// yielder-region sink (region planning rejects GOTOTABLE).
	const std::vector<int>* switch_pc = nullptr;
	const std::map<pc_t, CmpGroup>* cmp_groups;
	const std::map<pc_t, pc_t>* consumer_to_cmp;
	pc_t final_pc;
	// The entry function of a script that ends by falling off the end (global
	// init scripts have no QUIT) runs the script-end 0xFFFF handling.
	bool ffff_tail;
	// Otherwise, the function whose last block falls off its end, continuing at the next pc
	// (see emit_function_end). Null if it doesn't.
	const ZasmFunction* falls_off_fn = nullptr;

	// Term::Dispatch support, used only when this sink emits a structured
	// region nested inside the yielder's loop-switch (see YielderRegionPlan).
	// A dispatch terminator sets the dispatch target (l_idx_target_block) and branches out to the
	// enclosing dispatch loop.
	struct DispatchDesc
	{
		// The dispatch target to set: cfg block id + 1. Unused when is_return (RETURNFUNC exits to
		// the driver).
		int value = -1;
		// The terminator instruction's pc, for the debug marker / trace call.
		pc_t pc = 0;
		bool is_return = false;
		bool emit_debug = false;
	};
	// local block id -> descriptor. Null for non-yielder functions.
	const std::map<int, DispatchDesc>* dispatches = nullptr;
	// Depth of the enclosing dispatch loop from the region's scaffold position
	// (the structurer's live frame count is added per-branch).
	int dispatch_base_depth = 0;
	// Blocks >= this are synthesized dispatch trampolines with no body.
	int num_real_blocks = 0;

	pc_t block_final(int b) const
	{
		return b + 1 < (int)num_real_blocks ? (*starts)[b + 1] - 1 : final_pc;
	}

	void emit_block(int) override { state->wasm->emitBlock(); }
	void emit_loop(int) override { state->wasm->emitLoop(); }
	void emit_if() override { state->wasm->emitIf(); }
	void emit_else() override { state->wasm->emitElse(); }
	void emit_end() override { state->wasm->emitEnd(); }
	void emit_br(int depth, int) override { state->wasm->emitBr(depth); }
	void emit_br_if(int depth, int) override { state->wasm->emitBrIf(depth); }
	void emit_i32_eqz() override { state->wasm->emitI32Eqz(); }

	void emit_body(int b) override
	{
		WasmAssembler& wasm = *state->wasm;

		// Synthesized dispatch trampolines have no body.
		if (b >= num_real_blocks)
			return;

		for (int i = (*starts)[b]; i <= (*body_last)[b]; i++)
		{
			state->pc = i;
			int command = script->zasm[i].command;

#ifndef NDEBUG
			wasm.emitI32Const(i);
			wasm.emitDrop();
#endif

			if (state->runtime_debugging && !command_uses_comparison_result(command))
			{
				wasm.emitI32Const(i);
				emit_get_sp(*state);
				wasm.emitCall(state->f_idx_runtime_debug);
			}

			if (command_sets_comparison_result(command))
			{
				// Emit the operand snapshot (if any); the consumers emit as
				// they are reached - SETx below, a GOTOx terminator in
				// emit_cond. A loose compare with no consumers emits nothing.
				auto it = cmp_groups->find(i);
				if (it != cmp_groups->end())
					emit_cmp_capture(*state, script, it->second);
				continue;
			}

			if (auto it = consumer_to_cmp->find(i); it != consumer_to_cmp->end())
			{
				if (command == NOP)
					continue;
				// GOTOx consumers are always block terminators (emit_cond).
				CHECK(!command_is_goto_consumer(command));
				emit_cmp_set_consumer(*state, script, cmp_groups->at(it->second), i);
				continue;
			}

			switch (command)
			{
				case NOP:
					break;

				case CALLFUNC:
					// A direct call, or else the last command of the piece: a call through the
					// driver anywhere else is resumed after, which needs the loop-switch (see
					// compile_piece).
					compile_callfunc(*state, i, script->zasm[i].arg1);
					break;

				case GOTO:
					// Only the cross-function variant is body-emitted; intra-function GOTOs are
					// handled by the structurer.
					compile_goto_out_of_piece(*state, script->zasm[i].arg1);
					break;

				case QUIT:
					compile_quit(*state, script, i);
					break;

				case RETURNFUNC:
					// The return pc on the data stack is ignored (see emit_return).
					emit_return(*state);
					break;

				default:
				{
					if (!instruction_is_compiled(script->zasm[i]))
					{
						// Batch consecutive uncompiled commands into one
						// interpreter call, up to the end of this block.
						int count = 1;
						for (int j = i + 1; j <= (*body_last)[b]; j++)
						{
							if (instruction_is_compiled(script->zasm[j]))
								break;
							count += 1;
						}
						compile_command_interpreter(*state, script, i, count);
						i += count - 1;
						break;
					}

					compile_plain_command(*state, script, i);
				}
			}
		}

		int gpc = (*goto_debug_pc)[b];
		if (gpc >= 0)
		{
			state->pc = gpc;
#ifndef NDEBUG
			wasm.emitI32Const(gpc);
			wasm.emitDrop();
#endif
			if (state->runtime_debugging)
			{
				wasm.emitI32Const(gpc);
				emit_get_sp(*state);
				wasm.emitCall(state->f_idx_runtime_debug);
			}
		}

		// An Exit block leaves the function itself (see wasm_structurer.h), wherever the
		// structurer lays it out.
		if (b + 1 == (int)starts->size())
		{
			if (ffff_tail)
				emit_exit_stop(*state, RUNSCRIPT_STOPPED, script->size - 1); // See emit_function_end.
			else if (falls_off_fn)
				emit_function_end(*state, *structured_zasm, *falls_off_fn, false);
		}
	}

	void emit_cond(int b) override
	{
		// The block ends in a GOTOx comparison consumer; leave its take-branch
		// value (an i32, nonzero => branch taken) on the stack.
		pc_t term_pc = block_final(b);
		state->pc = term_pc;
		emit_cmp_goto_value(*state, script, cmp_groups->at(consumer_to_cmp->at(term_pc)), term_pc);
	}

	void emit_switch_dispatch(int b, const std::vector<int>& depths, int default_depth) override
	{
		CHECK(switch_pc);
		WasmAssembler& wasm = *state->wasm;
		pc_t term_pc = (*switch_pc)[b];
		state->pc = term_pc;
#ifndef NDEBUG
		wasm.emitI32Const((int)term_pc);
		wasm.emitDrop();
#endif
		if (state->runtime_debugging)
		{
			wasm.emitI32Const((int)term_pc);
			emit_get_sp(*state);
			wasm.emitCall(state->f_idx_runtime_debug);
		}

		if (script->zasm[term_pc].command == GOTOTABLE)
		{
			// A dense table indexes directly, which is what br_table does.
			emit_gototable_index(*state, script, term_pc);
			wasm.emitBrTable();
			wasm.emitVarU32(depths.size());
			for (int d : depths)
				wasm.emitVarU32(d);
			wasm.emitVarU32(default_depth);
			return;
		}

		// Ranges: a binary search, since br_table cannot express them. Every
		// `if` opened here nests the branch targets one deeper, so `extra`
		// tracks how many frames are open at each point (see emit_br_depth).
		emit_gotoranges_search(*state, script, term_pc, depths, default_depth);
	}

	void emit_dispatch(int b, int ctx_depth) override
	{
		// Only the yielder's structured regions use Term::Dispatch; a
		// non-yielding function has no dispatch to branch to.
		CHECK(dispatches);
		WasmAssembler& wasm = *state->wasm;
		const DispatchDesc& d = dispatches->at(b);

		if (d.emit_debug)
		{
			state->pc = d.pc;
#ifndef NDEBUG
			wasm.emitI32Const(d.pc);
			wasm.emitDrop();
#endif
			if (state->runtime_debugging)
			{
				wasm.emitI32Const(d.pc);
				emit_get_sp(*state);
				wasm.emitCall(state->f_idx_runtime_debug);
			}
		}

		if (d.is_return)
		{
			emit_return(*state);
			return;
		}

		wasm.emitI32Const(d.value);
		wasm.emitLocalSet(l_idx_target_block);

		// Branch out to the enclosing dispatch loop.
		wasm.emitBr(dispatch_base_depth + ctx_depth);
	}
};

// Refine a block partition so a control op is always the last instruction of
// its block: add a split after every control op the CFG doesn't split on
// (QUIT/RETURNFUNC never split a block, and a branch to a function's own
// entry is treated as a recursive call by zasm_construct_cfg, so it doesn't
// split either). `starts` holds the CFG's block starts on entry; sorted and
// unique on return.
static void refine_block_partition(const zasm_script* script, pc_t start_pc, pc_t final_pc, std::vector<pc_t>& starts)
{
	for (pc_t i = start_pc; i <= final_pc; i++)
	{
		int command = script->zasm[i].command;
		bool is_control = command == GOTO || command_is_goto_consumer(command) ||
						  command == QUIT || command == RETURNFUNC;
		if (is_control && i + 1 <= final_pc)
			starts.push_back(i + 1);
	}
	std::sort(starts.begin(), starts.end());
	starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
}

static int partition_block_id(const std::vector<pc_t>& starts, int pc)
{
	auto it = std::lower_bound(starts.begin(), starts.end(), (pc_t)pc);
	if (it == starts.end() || *it != (pc_t)pc)
		return -1;
	return it - starts.begin();
}

static pc_t partition_block_final(const std::vector<pc_t>& starts, pc_t final_pc, int b)
{
	return b + 1 < (int)starts.size() ? starts[b + 1] - 1 : final_pc;
}

// Comparison groups: map each compare and each of its consumers in
// [start_pc, final_pc], so SETx consumers emit in the body and a GOTOx
// terminator can re-derive its condition in emit_cond. A consumer run may
// span blocks (e.g. `COMPAREV; GOTOTRUE a; GOTOFALSE b`), which the per-pc
// map handles uniformly. Returns false if a run escapes the range.
static bool collect_cmp_groups(const zasm_script* script, pc_t start_pc, pc_t final_pc, std::map<pc_t, CmpGroup>& cmp_groups, std::map<pc_t, pc_t>& consumer_to_cmp)
{
	for (pc_t i = start_pc; i <= final_pc; i++)
	{
		int command = script->zasm[i].command;
		if (!command_sets_comparison_result(command))
			continue;

		CmpGroup g = analyze_comparison(script, i);
		if (g.consumers.empty())
			continue;

		if (g.consumers.back() > final_pc)
			return false;

		for (pc_t cj : g.consumers)
			consumer_to_cmp[cj] = i;

		i = g.consumers.back();
		cmp_groups.emplace(g.cmp_pc, std::move(g));
	}

	return true;
}

// Falling off the end of a piece. If the function goes on, continue in its next piece. The last
// function's end is the script's: its final command is a 0xFFFF that isn't part of the function
// (see zasm_construct_structured), which global init scripts rely on to end the script. Any other
// function falls through to the next command, as in the interpreter.
static void emit_function_end(CompilationState& state, const StructuredZasm& structured_zasm, const ZasmFunction& fn, bool end_function)
{
	if (state.final_pc != fn.final_pc)
		emit_exit_continue(state, state.final_pc + 1);
	else if (fn.id == structured_zasm.functions.back().id)
		emit_exit_stop(state, RUNSCRIPT_STOPPED, fn.final_pc + 1);
	else
		emit_exit_continue(state, fn.final_pc + 1);
	if (end_function)
		state.wasm->emitEnd(); // Function end.
}

static std::optional<WasmAssembler> compile_function_structured(CompilationState& state, const zasm_script* script, const StructuredZasm& structured_zasm, pc_t fn_id)
{
	const auto& fn = structured_zasm.functions[fn_id];
	pc_t start_pc = fn.start_pc;
	pc_t final_pc = fn.final_pc;

	for (pc_t i = start_pc; i <= final_pc; i++)
	{
		// Only for pieces with no resume points: nothing that leaves for the driver and comes back
		// (a call through the driver is fine as the last command, as nothing follows it).
		if (instruction_is_suspend(script->zasm[i]))
			return std::nullopt;
		if (script->zasm[i].command == CALLFUNC && i != final_pc && !is_direct_callable(structured_zasm, script->zasm[i].arg1))
			return std::nullopt;
	}

	auto cfg = zasm_construct_cfg(script, {{start_pc, final_pc}});
	std::vector<pc_t> starts = cfg.block_starts;
	refine_block_partition(script, start_pc, final_pc, starts);

	int n = starts.size();
	auto block_id_of = [&](int pc) { return partition_block_id(starts, pc); };
	auto block_final = [&](int b) { return partition_block_final(starts, final_pc, b); };

	std::map<pc_t, CmpGroup> cmp_groups;
	std::map<pc_t, pc_t> consumer_to_cmp;
	// A consumer run that leaves the function; shouldn't happen.
	if (!collect_cmp_groups(script, start_pc, final_pc, cmp_groups, consumer_to_cmp))
		return std::nullopt;

	// Classify each block's terminator (see wasm_structurer.h).
	std::vector<BlockInfo> infos(n);
	std::vector<int> body_last(n);
	std::vector<int> goto_debug_pc(n, -1);
	std::vector<int> switch_pc(n, -1);
	bool ffff_tail = false;
	bool falls_off_end = false;
	for (int b = 0; b < n; b++)
	{
		pc_t bfinal = block_final(b);
		int command = script->zasm[bfinal].command;
		int arg1 = script->zasm[bfinal].arg1;
		BlockInfo& bi = infos[b];
		body_last[b] = bfinal;

		if (command == GOTO)
		{
			if (arg1 >= (int)start_pc && arg1 <= (int)final_pc)
			{
				int target = block_id_of(arg1);
				if (target < 0)
					return std::nullopt;

				bi.term = Term::Uncond;
				bi.succ_true = target;
				body_last[b] = (int)bfinal - 1; // the structurer emits the branch
				goto_debug_pc[b] = (int)bfinal;
			}
			else if (structured_zasm.start_pc_to_function.contains(arg1))
			{
				bi.term = Term::Exit; // the body emits the tail-call + return
			}
			else
				return std::nullopt;
		}
		else if (command_is_goto_consumer(command))
		{
			if (!consumer_to_cmp.contains(bfinal))
				return std::nullopt; // a GOTOx with no reaching compare

			int target = arg1 >= (int)start_pc && arg1 <= (int)final_pc ? block_id_of(arg1) : -1;
			if (target < 0 || b + 1 >= n)
				return std::nullopt;

			bi.term = Term::Cond;
			bi.succ_true = target;
			bi.succ_false = b + 1;
			body_last[b] = (int)bfinal - 1; // emit_cond emits the condition
		}
		else if (command == GOTOTABLE || command == GOTORANGES)
		{
			// Multi-target dispatch (see defines.h), lowered by the structurer
			// via the sink: a br_table for a table, a compare tree for ranges.
			// succ_switch is {default, targets...}, matching zasm_jump_targets.
			bi.term = Term::Switch;
			for (int pc2 : zasm_jump_targets(command, script->literals.vec(script->zasm[bfinal])))
			{
				int target = pc2 >= (int)start_pc && pc2 <= (int)final_pc ? block_id_of(pc2) : -1;
				if (target < 0)
					return std::nullopt;
				bi.succ_switch.push_back(target);
			}
			body_last[b] = (int)bfinal - 1; // emit_switch_dispatch emits the dispatch
			switch_pc[b] = (int)bfinal;
		}
		else if (command == QUIT || command == RETURNFUNC)
		{
			bi.term = Term::Exit; // the body emits the trap/return
		}
		else if (b + 1 < n)
		{
			// No control op; the block exists because the next pc is a branch
			// target. Plain fall-through.
			bi.term = Term::Uncond;
			bi.succ_true = b + 1;
		}
		else
		{
			// Falls off the function's end. For the entry function of a script
			// whose zasm ends right after (global init scripts have no QUIT),
			// run the script-end 0xFFFF handling; otherwise this is simply an
			// implicit return.
			bi.term = Term::Exit;
			falls_off_end = true;
			if (fn_id == 0 && script->size - 1 == final_pc + 1 && script->zasm[script->size - 1].command == 0xFFFF)
				ffff_tail = true;
		}
	}

	WasmAssembler wasm;
	state.wasm = &wasm;
	wasm.reserve((final_pc - start_pc + 1) * 24); // see compile_piece
	emit_exec_prologue(state);

	StructuredFnSink sink{};
	sink.state = &state;
	sink.script = script;
	sink.structured_zasm = &structured_zasm;
	sink.starts = &starts;
	sink.body_last = &body_last;
	sink.goto_debug_pc = &goto_debug_pc;
	sink.switch_pc = &switch_pc;
	sink.cmp_groups = &cmp_groups;
	sink.consumer_to_cmp = &consumer_to_cmp;
	sink.final_pc = final_pc;
	sink.ffff_tail = ffff_tail;
	sink.falls_off_fn = falls_off_end ? &fn : nullptr;
	sink.num_real_blocks = n;

	// Irreducibility is detected before anything is emitted.
	WasmStructurer structurer(n, 0, infos);
	if (!structurer.analyze())
		return std::nullopt;
	structurer.emit(sink);

	// Every path has left the function by now.
	wasm.emitUnreachable();
	wasm.emitEnd(); // Function end.

	return wasm;
}

// Yielder loop structuring.
//
// The yielder must keep the loop-switch for its call/return/resume edges
// (runtime-valued targets), but a natural loop whose body contains no
// dispatch entry point - no wait, no call to a yielding function; a loop that
// always runs to completion without suspending - can be compiled as a real
// wasm loop nested inside the dispatch. The loop's interior blocks are
// "absorbed": they lose their scaffold frames (their br_table entries go
// dead - nothing dispatches to them at runtime), and the whole region is
// emitted structured in the header's scaffold segment. Edges leaving the
// region (breaks past the loop, returns) become Term::Dispatch: set
// l_idx_target_block and branch out to the dispatch loop.

struct YielderRegionPlan
{
	pc_t header_block; // global cfg block id; keeps its scaffold frame
	pc_t tail_block;   // last global cfg block id absorbed into the region
	pc_t start_pc;
	pc_t final_pc;

	// Local structured-region data, same shape compile_function_structured
	// builds for a whole function. infos may have synthesized dispatch
	// trampolines appended past starts.size().
	std::vector<pc_t> starts;
	std::vector<BlockInfo> infos;
	std::vector<int> body_last;
	std::vector<int> goto_debug_pc;
	std::map<int, StructuredFnSink::DispatchDesc> dispatches;
	std::map<pc_t, CmpGroup> cmp_groups;
	std::map<pc_t, pc_t> consumer_to_cmp;

	// Analyzed by detect_yielder_regions (whose reducibility check accepts the
	// region); emit_yielder_region reuses it instead of re-analyzing.
	std::unique_ptr<WasmStructurer> structurer;
};

// Builds the local structured-region plan for candidate loop [header..tail]
// (global cfg block ids). Returns nullopt for a shape the classifier doesn't
// model; the caller then just leaves that loop on the dispatch.
static std::optional<YielderRegionPlan> plan_yielder_region(const zasm_script* script, const ZasmCFG& cfg, pc_t header_block, pc_t tail_block)
{
	pc_t num_global_blocks = cfg.block_starts.size();

	YielderRegionPlan plan{};
	plan.header_block = header_block;
	plan.tail_block = tail_block;
	plan.start_pc = cfg.block_starts[header_block];
	plan.final_pc = tail_block + 1 < num_global_blocks ? cfg.block_starts[tail_block + 1] - 1 : cfg.final_pc;

	pc_t start_pc = plan.start_pc;
	pc_t final_pc = plan.final_pc;

	// Refined partition, same as compile_function_structured's.
	std::vector<pc_t>& starts = plan.starts;
	for (pc_t b = header_block; b <= tail_block; b++)
		starts.push_back(cfg.block_starts[b]);
	refine_block_partition(script, start_pc, final_pc, starts);

	int n = starts.size();
	auto block_id_of = [&](int pc) { return partition_block_id(starts, pc); };
	auto block_final = [&](int b) { return partition_block_final(starts, final_pc, b); };

	// The caller's eligibility scan already rejected runs escaping the
	// region; re-check defensively.
	if (!collect_cmp_groups(script, start_pc, final_pc, plan.cmp_groups, plan.consumer_to_cmp))
		return std::nullopt;

	// The dispatch value naming a global cfg block (same "+1" convention as
	// the flat lowering; the br_table's entry list maps values to depths).
	auto dispatch_value = [&](int target_pc) -> int {
		return (int)cfg.block_id_from_start_pc(target_pc) + 1;
	};

	plan.infos.resize(n);
	// Trampolines are appended during classification; reserve so BlockInfo
	// writes through plan.infos[b] stay valid.
	plan.infos.reserve(n * 3 + 2);
	plan.body_last.resize(n);
	plan.goto_debug_pc.assign(n, -1);

	auto add_trampoline = [&](int value, pc_t pc) -> int {
		int id = plan.infos.size();
		plan.infos.push_back({Term::Dispatch, -1, -1});
		plan.dispatches[id] = {value, pc, false, false};
		return id;
	};
	// The region's fall-out target: the global block right after it.
	auto fall_out_value = [&]() -> int {
		if (final_pc + 1 >= (pc_t)script->size || !cfg.contains_block_start(final_pc + 1))
			return -1;
		return dispatch_value(final_pc + 1);
	};

	for (int b = 0; b < n; b++)
	{
		pc_t bfinal = block_final(b);
		int command = script->zasm[bfinal].command;
		int arg1 = script->zasm[bfinal].arg1;
		plan.body_last[b] = bfinal;

		if (command == GOTO)
		{
			if (arg1 >= (int)start_pc && arg1 <= (int)final_pc)
			{
				int target = block_id_of(arg1);
				if (target < 0)
					return std::nullopt;
				plan.infos[b] = {Term::Uncond, target, -1};
				plan.body_last[b] = (int)bfinal - 1;
				plan.goto_debug_pc[b] = (int)bfinal;
			}
			else
			{
				// Leaves the region; the target is a block elsewhere in the
				// yielder (cross-function GOTOs out of the yielder are
				// rejected before compilation).
				if (arg1 < 0 || !cfg.contains_block_start(arg1))
					return std::nullopt;
				plan.infos[b] = {Term::Dispatch, -1, -1};
				plan.dispatches[b] = {dispatch_value(arg1), bfinal, false, true};
				plan.body_last[b] = (int)bfinal - 1;
			}
		}
		else if (command_is_goto_consumer(command))
		{
			if (!plan.consumer_to_cmp.contains(bfinal))
				return std::nullopt; // a GOTOx with no reaching compare
			int taken;
			if (arg1 >= (int)start_pc && arg1 <= (int)final_pc)
			{
				taken = block_id_of(arg1);
				if (taken < 0)
					return std::nullopt;
			}
			else
			{
				if (arg1 < 0 || !cfg.contains_block_start(arg1))
					return std::nullopt;
				taken = add_trampoline(dispatch_value(arg1), bfinal);
			}
			int fall;
			if (b + 1 < n)
				fall = b + 1;
			else
			{
				int value = fall_out_value();
				if (value < 0)
					return std::nullopt;
				fall = add_trampoline(value, bfinal);
			}
			plan.infos[b] = {Term::Cond, taken, fall};
			plan.body_last[b] = (int)bfinal - 1;
		}
		else if (command == QUIT)
		{
			plan.infos[b] = {Term::Exit, -1, -1}; // the body emits the trap
		}
		else if (command == RETURNFUNC)
		{
			// The continuation is popped from the return stack at runtime.
			plan.infos[b] = {Term::Dispatch, -1, -1};
			plan.dispatches[b] = {-1, bfinal, true, true};
			plan.body_last[b] = (int)bfinal - 1;
		}
		else if (b + 1 < n)
		{
			plan.infos[b] = {Term::Uncond, b + 1, -1};
		}
		else
		{
			// Falls out of the region.
			int value = fall_out_value();
			if (value < 0)
				return std::nullopt;
			plan.infos[b] = {Term::Dispatch, -1, -1};
			plan.dispatches[b] = {value, block_final(b), false, false};
		}
	}

	return plan;
}

// Emits an accepted region as structured wasm at its scaffold position.
static void emit_yielder_region(CompilationState& state, const zasm_script* script, const StructuredZasm& structured_zasm, const YielderRegionPlan& plan, int dispatch_base_depth)
{
	StructuredFnSink sink{};
	sink.state = &state;
	sink.script = script;
	sink.structured_zasm = &structured_zasm;
	sink.starts = &plan.starts;
	sink.body_last = &plan.body_last;
	sink.goto_debug_pc = &plan.goto_debug_pc;
	sink.cmp_groups = &plan.cmp_groups;
	sink.consumer_to_cmp = &plan.consumer_to_cmp;
	sink.final_pc = plan.final_pc;
	sink.ffff_tail = false;
	sink.dispatches = &plan.dispatches;
	sink.dispatch_base_depth = dispatch_base_depth;
	sink.num_real_blocks = (int)plan.starts.size();

	plan.structurer->emit(sink);
}

// Finds the yielder's eligible loops. Conservative by construction: anything
// rejected simply stays on the loop-switch.
// Dispatch entry points: blocks the loop-switch's br_table can be asked to land on at runtime -
// the piece and script entries, and the blocks after calls and suspends, where the driver resumes
// the piece (see pc_to_resume_address). Yielder regions must not absorb these (see
// detect_yielder_regions).
static std::set<pc_t> compute_dispatch_entries(const zasm_script* script, const StructuredZasm& structured_zasm, const ZasmCFG& cfg, const std::vector<std::pair<pc_t, pc_t>>& pc_ranges)
{
	std::set<pc_t> dispatch_entries;
	for (auto [start_pc, final_pc] : pc_ranges)
		dispatch_entries.insert(cfg.block_id_from_start_pc(start_pc));
	for (auto sd : script->script_datas)
		if (cfg.contains_block_start(sd->pc))
			dispatch_entries.insert(cfg.block_id_from_start_pc(sd->pc));
	for (auto [start_pc, final_pc] : pc_ranges)
	{
		for (pc_t i = start_pc; i <= final_pc; i++)
		{
			int command = script->zasm[i].command;
			// A call's return continuation, for a call through the driver.
			bool resume_after = instruction_is_suspend(script->zasm[i]) ||
				(command == CALLFUNC && !is_direct_callable(structured_zasm, script->zasm[i].arg1));
			if (resume_after && i + 1 <= final_pc && cfg.contains_block_start(i + 1))
				dispatch_entries.insert(cfg.block_id_from_start_pc(i + 1));
		}
	}
	return dispatch_entries;
}

static std::vector<YielderRegionPlan> detect_yielder_regions(const zasm_script* script, const StructuredZasm& structured_zasm, const ZasmCFG& cfg, const std::vector<std::pair<pc_t, pc_t>>& pc_ranges)
{
	pc_t num_blocks = cfg.block_starts.size();
	auto block_final = [&](pc_t b) -> pc_t {
		return b + 1 < num_blocks ? cfg.block_starts[b + 1] - 1 : cfg.final_pc;
	};

	// These must keep their scaffold frames, so no region may absorb them (a
	// region may still *start* at one - dispatching to the header lands at the
	// top of the structured loop).
	std::set<pc_t> dispatch_entries = compute_dispatch_entries(script, structured_zasm, cfg, pc_ranges);

	// Candidate loops: for each backward edge b -> h, take the largest such b
	// per header h. Also record each block's predecessor bounds; the
	// single-entry check below only needs "do all of an interior block's
	// predecessors lie within the region", which min/max answer in O(1).
	std::map<pc_t, pc_t> header_to_tail;
	std::vector<pc_t> min_pred(num_blocks, (pc_t)-1);
	std::vector<pc_t> max_pred(num_blocks, 0);
	for (pc_t b = 0; b < num_blocks; b++)
	{
		for (pc_t e : cfg.block_edges[b])
		{
			if (e <= b)
				header_to_tail[e] = std::max(header_to_tail[e], b);
			min_pred[e] = std::min(min_pred[e], b);
			max_pred[e] = std::max(max_pred[e], b);
		}
	}

	std::vector<YielderRegionPlan> plans;
	for (auto [h, t] : header_to_tail)
	{
		// Overlapping an accepted region: it's an inner loop, and the region's
		// own structurer already handles it.
		if (!plans.empty() && h <= plans.back().tail_block)
			continue;
		// The last block of the yielder is excluded so the fall-off-the-end
		// script exit (see the 0xFFFF handling below) stays reachable.
		if (t + 1 >= num_blocks)
			continue;

		// Must lie within a single function's range.
		pc_t h_pc = cfg.block_starts[h];
		pc_t t_final = block_final(t);
		bool same_range = false;
		for (auto [start_pc, final_pc] : pc_ranges)
			if (h_pc >= start_pc && t_final <= final_pc)
				same_range = true;
		if (!same_range)
			continue;

		// Interior blocks must not be dispatch entries.
		bool ok = true;
		for (pc_t x = h + 1; x <= t && ok; x++)
			if (dispatch_entries.contains(x))
				ok = false;

		// The loop must run to completion without leaving the piece: no waits, no
		// frozen-script runs, no calls through the driver.
		// Comparison-consumer runs must not escape the region.
		for (pc_t i = h_pc; i <= t_final && ok; i++)
		{
			int command = script->zasm[i].command;
			if (instruction_is_suspend(script->zasm[i]))
				ok = false;
			else if (command == GOTOTABLE || command == GOTORANGES)
				ok = false; // region planning has no Term::Switch support; the flat lowering handles it
			else if (command == CALLFUNC && !is_direct_callable(structured_zasm, script->zasm[i].arg1))
				ok = false; // resumed after, through the driver
			else if (command_sets_comparison_result(command))
			{
				CmpGroup g = analyze_comparison(script, i);
				if (!g.consumers.empty())
				{
					if (g.consumers.back() > t_final)
						ok = false;
					else
						i = g.consumers.back();
				}
			}
		}

		// Single-entry: no edges from outside the region into its interior.
		// (min_pred of (pc_t)-1 means "no predecessors" and passes trivially.)
		for (pc_t x = h + 1; x <= t && ok; x++)
			if (min_pred[x] < h || max_pred[x] > t)
				ok = false;
		if (!ok)
			continue;

		auto plan = plan_yielder_region(script, cfg, h, t);
		if (!plan)
			continue;
		plan->structurer = std::make_unique<WasmStructurer>((int)plan->infos.size(), 0, plan->infos);
		if (!plan->structurer->analyze())
			continue; // irreducible

		plans.push_back(std::move(*plan));
	}

	// A comparison outside every region must not have a consumer run leaking
	// into one (the flat lowering would try to close absorbed frames). Drop
	// any region a run reaches into. plans is sorted by start_pc, so a single
	// cursor tracks the region ahead of the scan position.
	std::set<size_t> dropped;
	size_t next_plan = 0;
	for (auto [start_pc, final_pc] : pc_ranges)
	{
		for (pc_t i = start_pc; i <= final_pc; i++)
		{
			while (next_plan < plans.size() && plans[next_plan].final_pc < i)
				next_plan++;
			if (next_plan < plans.size() && i >= plans[next_plan].start_pc)
			{
				// Inside a region; its own classification already vetted it.
				i = plans[next_plan].final_pc;
				continue;
			}
			int command = script->zasm[i].command;
			if (!command_sets_comparison_result(command))
				continue;
			CmpGroup g = analyze_comparison(script, i);
			if (g.consumers.empty())
				continue;
			if (next_plan < plans.size() && g.consumers.back() >= plans[next_plan].start_pc)
				dropped.insert(next_plan);
			i = g.consumers.back();
		}
	}
	if (!dropped.empty())
	{
		std::vector<YielderRegionPlan> kept;
		for (size_t r = 0; r < plans.size(); r++)
			if (!dropped.contains(r))
				kept.push_back(std::move(plans[r]));
		plans = std::move(kept);
	}

	return plans;
}

// Compiles a piece (state.start_pc..state.final_pc) of `fn` to the body of exec(ctx), lowered as a
// loop-switch over its blocks: a call, wait or suspend leaves exec for the driver, which re-enters
// it after - at a dispatch entry, whose dispatch value is recorded in pc_to_resume (keyed by the pc
// before it, as the driver looks it up).
static WasmAssembler compile_piece(CompilationState& state, const zasm_script* script, const StructuredZasm& structured_zasm, const ZasmFunction& fn, std::map<pc_t, uintptr_t>& pc_to_resume)
{
	WasmAssembler wasm;
	state.wasm = &wasm;

	std::vector<std::pair<pc_t, pc_t>> pc_ranges = {{state.start_pc, state.final_pc}};
	// ~24 bytes of wasm per ZASM instruction (measured); over-reserving is fine,
	// the buffer is freed right after the module is assembled.
	wasm.reserve((state.final_pc - state.start_pc + 1) * 24);
	auto cfg = zasm_construct_cfg(script, pc_ranges, true);

	// Dispatch values are (cfg block id + 1), matching what the jumps below set. The piece's start
	// is entered with no resume address (dispatch value 0, which lands at the first block).
	for (pc_t b : compute_dispatch_entries(script, structured_zasm, cfg, pc_ranges))
	{
		pc_t pc = cfg.block_starts[b];
		if (pc != state.start_pc)
			pc_to_resume[pc - 1] = b + 1;
	}

	// All control flow is converted to a simple, but slow, loop-switch construct: a giant switch case,
	// implemented with "loop", "br_table", and a "block" for every block in the CFG.
	//
	// See: https://medium.com/leaningtech/solving-the-structured-control-flow-problem-once-and-for-all-5123117b1ee2#:~:text=A%20universal%20but%20inefficient%20solution
	//
	// Pieces with no resume points don't use this - they get real structured control flow via
	// compile_function_structured (which falls back to this lowering for irreducible CFGs, and
	// -no-jit-wasm-structured forces it everywhere). The rest keep the dispatch for their
	// resume edges; but their loops that never leave the piece are compiled as real wasm loops
	// nested inside the dispatch - see the region detection just below, and
	// https://dl.acm.org/doi/pdf/10.1145/3547621 for the general technique.

	// -jit-wasm-structured-yielder: compile the yielder's non-suspending loops
	// as real wasm loops nested inside the dispatch (see YielderRegionPlan).
	// With no accepted regions this lowering is identical to the flat one.
	static bool yielder_structured = get_flag_bool("-jit-wasm-structured-yielder").value_or(true);
	std::vector<YielderRegionPlan> regions;
	if (yielder_structured)
		regions = detect_yielder_regions(script, structured_zasm, cfg, pc_ranges);

	pc_t num_blocks = cfg.block_starts.size();

	// Interior blocks of accepted regions are absorbed: no scaffold frame, and
	// their br_table entries go dead (nothing dispatches to them at runtime).
	std::vector<uint8_t> absorbed(num_blocks, 0);
	std::vector<pc_t> absorbed_header(num_blocks, 0);
	std::map<pc_t, const YielderRegionPlan*> region_by_header;
	for (auto& r : regions)
	{
		region_by_header[r.header_block] = &r;
		for (pc_t b = r.header_block + 1; b <= r.tail_block; b++)
		{
			absorbed[b] = 1;
			absorbed_header[b] = r.header_block;
		}
	}
	// Scaffold rank of each non-absorbed block. In the flat case rank == id,
	// and every depth/entry below reduces to the classic loop-switch values.
	std::vector<int> rank(num_blocks, -1);
	int num_frames = 0;
	for (pc_t b = 0; b < num_blocks; b++)
		if (!absorbed[b])
			rank[b] = num_frames++;

	emit_exec_prologue(state);

	wasm.emitLoop();
	for (pc_t i = 0; i < num_blocks; i++)
	{
		if (!absorbed[i])
			wasm.emitBlock();
	}

	// Jump to the next block.
	// For the first execution of this function, this initially jumps to the first block.
	// For subsequent executions (re-entering after `WaitX`), it initially jumps to the last block yielded from.
	// For subsequent loop iterations in the same call to the script, it jumps to the block specified by the
	// previous block. This is what handles branches (including if, calls, loops) in the CFG. If a block simply
	// falls through to the next, it does so directly (no additional loop iteration).
	//
	// Dispatch values are (cfg block id + 1); the entry list maps each value to
	// the branch depth landing at that block's code, which is (scaffold rank +
	// 1) because frames close innermost-first while code advances forward.
	wasm.emitBlock();
	wasm.emitLocalGet(l_idx_target_block);
	wasm.emitBrTable();
	// Plus 2, because of the inner-most block (this most recently emitted one), and
	// the outer-most loop.
	pc_t br_table_len = num_blocks + 2;
	wasm.emitVarU32(br_table_len);
	for (pc_t v = 0; v < br_table_len; v++)
	{
		if (v == 0)
			wasm.emitVarU32(0);
		else if (v == num_blocks + 1)
			wasm.emitVarU32(num_frames + 1); // the dispatch loop itself
		else
		{
			pc_t j = v - 1;
			// Absorbed blocks are never dispatched to; route their dead
			// entries at the region header, which is harmless and in range.
			pc_t land = absorbed[j] ? absorbed_header[j] : j;
			wasm.emitVarU32(rank[land] + 1);
		}
	}
	wasm.emitVarU32(0);
	wasm.emitEnd();

	// Jumps to `pc` from a scaffold position `depth` frames inside the dispatch loop.
	auto emit_jump = [&](int pc, int depth) {
		if (pc < (int)state.start_pc || pc > (int)state.final_pc)
		{
			emit_exit_continue(state, pc);
			return;
		}

		wasm.emitI32Const(cfg.block_id_from_start_pc(pc) + 1);
		wasm.emitLocalSet(l_idx_target_block);
		wasm.emitBr(depth);
	};

	// (cfg block id of the current block) + 1 while emitting its code.
	pc_t current_block_index = 0;
	// (scaffold rank of the current block) + 1 while emitting its code. The
	// dispatch loop's branch depth from anywhere is num_frames - current_rank.
	int current_rank = 0;

	// During block b's body current_block_index == b + 1, so the next block
	// start is always block_starts[current_block_index] - an O(1) check in this
	// per-instruction loop, where contains_block_start's out-of-line binary
	// search showed up in compile-time profiles.
	auto next_block_start = [&]() -> pc_t {
		return current_block_index < num_blocks ? cfg.block_starts[current_block_index] : (pc_t)-1;
	};

	for (auto [start_pc, final_pc] : pc_ranges)
	{
		for (pc_t i = start_pc; i <= final_pc; i++)
		{
			state.pc = i;

			if (i == next_block_start())
			{
				wasm.emitEnd();
				current_block_index += 1;
				current_rank += 1;

				// A structured region: emit the whole loop as real control
				// flow in this scaffold segment, then continue after it.
				if (auto it = region_by_header.find(current_block_index - 1); it != region_by_header.end())
				{
					const YielderRegionPlan& plan = *it->second;
					emit_yielder_region(state, script, structured_zasm, plan, num_frames - current_rank);
					i = plan.final_pc; // the loop's ++ resumes right after the region
					current_block_index = plan.tail_block + 1;
					continue;
				}
			}

#ifndef NDEBUG
			wasm.emitI32Const(i);
			wasm.emitDrop();
#endif

			int command = script->zasm[i].command;
			int arg1 = script->zasm[i].arg1;

			if (state.runtime_debugging && !command_uses_comparison_result(command))
			{
				wasm.emitI32Const(i);
				emit_get_sp(state);
				wasm.emitCall(state.f_idx_runtime_debug);
			}

			if (command_sets_comparison_result(command))
			{
				i = compile_comparison(state, script, cfg, num_frames, current_rank, current_block_index, i);
				continue;
			}

			if (command_is_wait(command))
			{
				// Wait commands normally yield back to the engine, however there are some
				// special cases where it does not. For example, when WAITFRAMESR arg is 0.
				// This exits to the driver, but only if actually waiting.
				compile_command_interpreter(state, script, i, 1, true);
				continue;
			}

			// RUNGENFRZSCR runs frozen engine frames (a nested generic script) inline,
			// as the interpreter does. Those frames suspend the program (JSPI) with this
			// script's frames still on the stack, and can run other scripts - including
			// this piece (nothing of a running piece lives outside its locals).
			if (command == RUNGENFRZSCR)
			{
				compile_command_interpreter(state, script, i, 1);
				continue;
			}

			// The other suspends (see instruction_is_suspend): an lweapon script lifting itself
			// stops for the rest of the frame. The interpreter leaves ri->pc where it resumes
			// (the next block), and compile_command_interpreter exits with it.
			if (instruction_is_suspend(script->zasm[i]))
			{
				compile_command_interpreter(state, script, i, 1, false, true);
				continue;
			}

			if (!instruction_is_compiled(script->zasm[i]))
			{
				// Every command that is not compiled to WASM must go through the regular interpreter function.
				// In order to reduce function call overhead, we call into the interpreter function in batches.
				int uncompiled_command_count = 1;
				for (pc_t j = i + 1; j < (pc_t)script->size; j++)
				{
					if (instruction_is_compiled(script->zasm[j]))
						break;
					if (j == next_block_start()) // O(1) contains_block_start; see above
						break;
					if (instruction_is_suspend(script->zasm[j]))
						break;

					uncompiled_command_count += 1;
				}

				compile_command_interpreter(state, script, i, uncompiled_command_count);
				i += uncompiled_command_count - 1;
				continue;
			}

			// Every command here must be reflected in command_is_compiled!
			switch (command)
			{
				case NOP: break;

				case CALLFUNC:
				{
					if (is_direct_callable(structured_zasm, arg1))
					{
						compile_callfunc_direct(state, i, arg1);
						break;
					}

					compile_callfunc_driver(state, i, arg1);
					// Resumed after the callee returns, at the next block (a dispatch entry).
					CHECK(i == state.final_pc || cfg.contains_block_start(i + 1));
				}
				break;

				case GOTO:
				{
					if (arg1 < (int)state.start_pc || arg1 > (int)state.final_pc)
					{
						compile_goto_out_of_piece(state, arg1);
						continue;
					}

					// Set the target block index and jump back to the loop-switch.
					wasm.emitI32Const(cfg.block_id_from_start_pc(arg1) + 1);
					wasm.emitLocalSet(l_idx_target_block);
					wasm.emitBr(num_frames - current_rank);
				}
				break;

				case GOTORANGES:
				{
					// Range search; see the opcode's comment in defines.h. As with
					// GOTOTABLE below, the loop-switch reaches every target through
					// the dispatch variable.
					const auto& table = *script->literals.vec(script->zasm[i]);
					int base_depth = num_frames - current_rank;
					emit_gotoranges_tree(state, script, i, [&](int target_index, int extra) {
						int target_pc = target_index < 0 ? table[0] : gotoranges_at(table, target_index).target;
						emit_jump(target_pc, base_depth + extra);
					});
				}
				break;

				case GOTOTABLE:
				{
					// Dense-switch dispatch; see the opcode's comment in defines.h.
					// In the loop-switch lowering every branch goes through the
					// dispatch variable, so lower to a balanced compare tree over
					// the table index whose leaves set the dispatch target. (The
					// structured lowering uses a real br_table instead.)
					const auto& table = *script->literals.vec(script->zasm[i]);
					size_t num_targets = table.size() - 2;
					auto runs = gototable_compress(table);
					int base_depth = num_frames - current_rank;

					auto emit_jump_to = [&](int pc, int extra) {
						emit_jump(pc, base_depth + extra);
					};

					if (num_targets == 0)
					{
						emit_jump_to(table[1], 0);
						break;
					}

					emit_gototable_index(state, script, i);
					wasm.emitLocalSet(l_idx_scratch2);

					// Out of [0, n) - including the invalid-key -1 - takes the default.
					wasm.emitLocalGet(l_idx_scratch2);
					wasm.emitI32Const((int)num_targets);
					wasm.emitI32GeU();
					wasm.emitIf();
					emit_jump_to(table[1], 1);
					wasm.emitEnd();

					// Binary search over runs of equal targets.
					auto emit_tree = [&](auto&& self, size_t lo, size_t hi, int extra) -> void {
						if (lo == hi)
						{
							emit_jump_to(runs[lo].target, extra);
							return;
						}
						size_t mid = (lo + hi + 1) / 2;
						wasm.emitLocalGet(l_idx_scratch2);
						wasm.emitI32Const(runs[mid].start);
						wasm.emitI32GeS();
						wasm.emitIf();
						self(self, mid, hi, extra + 1);
						wasm.emitEnd();
						self(self, lo, mid - 1, extra);
					};
					emit_tree(emit_tree, 0, runs.size() - 1, 0);
				}
				break;

				case RETURNFUNC:
				{
					// The return pc on the data stack is ignored (see emit_return).
					if (command == RETURN)
						add_sp(state, 1);
					emit_return(state);
				}
				break;

				case QUIT:
					compile_quit(state, script, i);
					break;

				default:
					compile_plain_command(state, script, i);
			}
		}
	}

	// Falling off the end of the piece.
	emit_function_end(state, structured_zasm, fn, false);

	// Finish loop construct.
	wasm.emitBr(0); // Unconditional branch to top of loop.
	wasm.emitEnd(); // Loop end.

	wasm.emitUnreachable(); // Every path exits above.
	wasm.emitEnd(); // Function end.

	return wasm;
}

// Each piece compiles to its own module (see compile_piece). Codegen touches only the script and
// local state, so it runs on any thread; the browser compiles and instantiates the module, on the
// main thread, before the function is committed (see jit_backend_prepare_commits).
struct WasmPendingModule
{
	std::string name;
	WasmModule wm;
	// The browser compile of a batch of modules, started by jit_backend_prepare_commits.
	struct Batch
	{
		int id = 0;
		bool done = false;
		std::vector<int> run_fns;
	};
	std::shared_ptr<Batch> batch;
	size_t index_in_batch = 0;
};

static std::optional<WasmModule> wasm_codegen_piece_impl(zasm_script* script, JittedScript* j_script, pc_t piece_id, std::map<pc_t, uintptr_t>& pc_to_resume, std::string& module_name)
{
	const JitPiece& piece = j_script->pieces[piece_id];
	const StructuredZasm& structured_zasm = j_script->structured_zasm;
	const ZasmFunction& fn = structured_zasm.functions[piece.fn_id];

	if (!structured_zasm.is_modern_function_calling())
	{
		codegen_trace(fmt::format("[jit] NOT compiling zasm chunk (unexpected function call mode): {} id: {} size: {}\n", script->name, script->id, script->size));
		return std::nullopt;
	}

	// STACKWRITEATVV_IF is not yet supported by the wasm codegen - always bail.
	for (pc_t i = piece.start_pc; i <= piece.final_pc; i++)
	{
		if (script->zasm[i].command == STACKWRITEATVV_IF)
		{
			error(script, "STACKWRITEATVV_IF unsupported", true);
			return std::nullopt;
		}
	}

	WasmCompiler comp{};
	CompilationState state{};
	state.j_script = j_script;
	state.start_pc = piece.start_pc;
	state.final_pc = piece.final_pc;
	state.runtime_debugging = script_debug_is_runtime_debugging() == 2;
	// Keep ri->pc current only when a stack trace could actually be produced (its only
	// consumer), so non-tracing runs pay nothing per engine call.
	state.maintain_call_stack = FFCore.should_display_stack_traces();
	state.inline_regs = get_flag_bool("-jit-wasm-inline-regs").value_or(true);

	state.f_idx_do_commands = comp.builder.importFunction("do_commands", 4, 1);
	state.f_idx_direct_enter = comp.builder.importFunction("direct_enter", 2, 0);
	state.f_idx_direct_retstack_pop = comp.builder.importFunction("direct_retstack_pop", 0, 0);
	state.f_idx_get_register = comp.builder.importFunction("get_register", 1, 1);
	state.f_idx_set_register = comp.builder.importFunction("set_register", 2, 0);
	state.f_idx_set_guarded_register = comp.builder.importFunction("set_guarded_register", 3, 0);
	state.f_idx_runtime_debug = comp.builder.importFunction("runtime_debug", 2, 0);
	state.f_idx_log_error = comp.builder.importFunction("log_error", 1, 0);
	state.f_idx_pod_read = comp.builder.importFunction("pod_read", 4, 1);
	state.f_idx_pod_write = comp.builder.importFunction("pod_write", 6, 0);
	state.f_idx_allocatemem = comp.builder.importFunction("allocatemem", 3, 1);
	state.f_idx_writepodarr = comp.builder.importFunction("writepodarr", 2, 0);
	state.f_idx_class_read = comp.builder.importFunction("class_read", 3, 1);
	state.f_idx_class_write = comp.builder.importFunction("class_write", 4, 0);
	state.f_idx_string_compare = comp.builder.importFunction("string_compare", 4, 1);
	state.f_idx_ref_change = comp.builder.importFunction("ref_change", 4, 0);
	state.f_idx_mark_type_stack = comp.builder.importFunction("mark_type_stack", 2, 0);
	state.f_idx_arraysize = comp.builder.importFunction("arraysize", 2, 1);

	uint32_t fn_exec_idx = comp.builder.declareFunction({WasmValType::I32}, {WasmValType::I32});
	comp.builder.exportFunction(fn_exec_idx, "exec");
	state.exec_type = comp.builder.internFuncType({WasmValType::I32}, {WasmValType::I32});
	comp.builder.import_table = true;

	// A piece that is a whole function and never leaves for the driver mid-way gets real
	// structured control flow (falling back to the loop-switch for irreducible CFGs);
	// -no-jit-wasm-structured forces the loop-switch everywhere, for A/B runs and bisection.
	static bool structured = get_flag_bool("-jit-wasm-structured").value_or(true);
	std::optional<WasmAssembler> wasm;
	if (structured && piece.start_pc == fn.start_pc && piece.final_pc == fn.final_pc)
		wasm = compile_function_structured(state, script, structured_zasm, fn.id);
	if (!wasm)
		wasm = compile_piece(state, script, structured_zasm, fn, pc_to_resume);

	comp.builder.defineFunction(fn_exec_idx, std::vector<WasmValType>(num_exec_locals, WasmValType::I32), wasm->finish());
	comp.builder.setFunctionName(fn_exec_idx, fn.name());

	// Unique per piece: function names repeat (overloads), and -jit-save-wasm writes a file per
	// module, possibly from many threads at once.
	module_name = fmt::format("{}-{}-{}", script->name, fn.name(), piece.start_pc);
	comp.builder.moduleName = module_name;
	auto wm = comp.finish();

	static bool write_to_disk = get_flag_bool("-jit-save-wasm").value_or(false);
	if (write_to_disk)
	{
		fs::path folder = fs::current_path() / "wasm" / get_filename(qstpath);
		fs::create_directories(folder);
		fs::path filename = folder / fmt::format("{}.wasm", module_name);
		std::ofstream outfile(filename, std::ios::out | std::ios::binary);
		outfile.write(reinterpret_cast<const char*>(wm.data.data()), wm.data.size());
	}

	return wm;
}

// Codegen often runs on a worker thread (see compile_and_queue_piece), where an uncaught
// exception kills the worker and the browser logs only "Uncaught [object Object]". Report it as a
// compile failure instead, so the message is logged and the function stays interpreted.
static std::optional<WasmModule> wasm_codegen_piece(zasm_script* script, JittedScript* j_script, pc_t piece_id, std::map<pc_t, uintptr_t>& pc_to_resume, std::string& module_name)
{
	try
	{
		return wasm_codegen_piece_impl(script, j_script, piece_id, pc_to_resume, module_name);
	}
	catch (const std::exception& e)
	{
		error(script, fmt::format("exception: {}", e.what()));
		return std::nullopt;
	}
}

std::optional<JittedFunction> jit_backend_compile_function(zasm_script* script, JittedScript* j_script, pc_t piece_id)
{
	std::string log;
	codegen_log = &log;

	std::map<pc_t, uintptr_t> pc_to_resume;
	std::string module_name;
	auto wm = wasm_codegen_piece(script, j_script, piece_id, pc_to_resume, module_name);

	codegen_log = nullptr;
	if (!log.empty() && jit_log_is_enabled())
		jit_trace(log);

	if (!wm)
		return std::nullopt;

#ifndef __EMSCRIPTEN__
	// Native builds with this backend can only generate the module (e.g. for -jit-save-wasm
	// validation), not run it.
	return std::nullopt;
#else
	auto pending = std::make_shared<WasmPendingModule>();
	pending->name = std::move(module_name);
	pending->wm = std::move(*wm);
	return JittedFunction{
		.exec = nullptr,
		.id = piece_id,
		.pc_to_resume_address = std::move(pc_to_resume),
		.backend_data = std::move(pending),
	};
#endif
}

// Starts one browser compile for every function given that isn't being compiled yet. Returns its
// batch, or null if there was nothing to compile.
[[maybe_unused]] static std::shared_ptr<WasmPendingModule::Batch> start_browser_compiles(const std::vector<JittedFunction*>& fns)
{
	std::vector<WasmPendingModule*> modules;
	for (auto j_fn : fns)
	{
		auto module = static_cast<WasmPendingModule*>(j_fn->backend_data.get());
		if (module && !module->batch)
			modules.push_back(module);
	}
	if (modules.empty())
		return nullptr;

	auto batch = std::make_shared<WasmPendingModule::Batch>();
	batch->run_fns.resize(modules.size());
	std::vector<const char*> names;
	std::vector<void*> ptrs;
	std::vector<uint32_t> sizes;
	for (size_t i = 0; i < modules.size(); i++)
	{
		modules[i]->batch = batch;
		modules[i]->index_in_batch = i;
		names.push_back(modules[i]->name.c_str());
		ptrs.push_back(modules[i]->wm.data.data());
		sizes.push_back(modules[i]->wm.data.size());
	}

	batch->id = em_compile_wasm_batch_start(modules.size(), names.data(), ptrs.data(), sizes.data());
	return batch;
}

// Returns whether the batch has finished compiling.
[[maybe_unused]] static bool poll_browser_compile(WasmPendingModule::Batch& batch)
{
	if (!batch.done && em_compile_wasm_batch_poll(batch.id, batch.run_fns.data()))
		batch.done = true;
	return batch.done;
}

// Sets j_fn.exec once its browser compile is done (see poll_browser_compile). Returns false if the
// browser failed to compile it (it stays interpreted).
[[maybe_unused]] static bool finish_browser_compile(JittedFunction& j_fn)
{
	auto module = static_cast<WasmPendingModule*>(j_fn.backend_data.get());
	if (!module || j_fn.exec || !module->batch || !module->batch->done)
		return true;

	auto& batch = *module->batch;

	int run_fn = batch.run_fns[module->index_in_batch];
	if (!run_fn)
	{
		jit_error("[jit ERROR] the browser failed to compile %s\n", module->name.c_str());
		return false;
	}

	j_fn.exec = reinterpret_cast<JittedFunctionImpl>(static_cast<uintptr_t>(run_fn));
	j_fn.backend_data.reset(); // The module bytes aren't needed anymore.
	return true;
}

[[maybe_unused]] static void finish_browser_compiles(std::deque<JittedFunction>& pending)
{
	// Poll each batch once, rather than once per function in it.
	std::set<WasmPendingModule::Batch*> polled;
	for (auto& j_fn : pending)
	{
		auto module = static_cast<WasmPendingModule*>(j_fn.backend_data.get());
		if (module && module->batch && polled.insert(module->batch.get()).second)
			poll_browser_compile(*module->batch);
	}

	std::erase_if(pending, [](JittedFunction& j_fn){ return !finish_browser_compile(j_fn); });
}

void jit_backend_prepare_commits([[maybe_unused]] JittedScript* j_script, [[maybe_unused]] std::deque<JittedFunction>& pending)
{
#ifdef __EMSCRIPTEN__
	std::vector<JittedFunction*> fns;
	for (auto& j_fn : pending)
		fns.push_back(&j_fn);
	start_browser_compiles(fns);
	finish_browser_compiles(pending);
#endif
}

void jit_backend_release_function(JittedFunction& j_fn)
{
	if (j_fn.exec)
	{
		em_destroy_wasm_module(static_cast<int>(reinterpret_cast<uintptr_t>(j_fn.exec)));
		return;
	}

	// Still in a browser compile, which may cover other functions of the same script being
	// released too. If done, release this module; otherwise drop the batch, which releases its
	// modules once compiled.
	auto module = static_cast<WasmPendingModule*>(j_fn.backend_data.get());
	if (!module || !module->batch)
		return;
	auto& batch = *module->batch;
	if (batch.done)
	{
		if (int run_fn = batch.run_fns[module->index_in_batch])
			em_destroy_wasm_module(run_fn);
	}
	else if (batch.id)
	{
		em_compile_wasm_batch_cancel(batch.id);
		batch.id = 0;
	}
}

// Precompile: set up every script (which, with precompiling on, codegens each of its functions) on
// the engine worker pool, then compile every module in the browser in ONE batch
// (em_compile_wasm_batch) before the game starts, letting the browser compile them in parallel on
// its own pool. The functions are committed on their script's first run.
bool jit_precompile_scripts_impl([[maybe_unused]] const std::vector<zasm_script*>& scripts, [[maybe_unused]] const std::function<void(zasm_script*, JittedScript*)>& on_compiled)
{
#ifndef __EMSCRIPTEN__
	return false;
#else
	std::vector<JittedScript*> j_scripts(scripts.size());
	if (auto worker_pool = zasm_pipeline_worker_pool())
	{
		for (size_t i = 0; i < scripts.size(); i++)
		{
			worker_pool->add_task([&, i](){
				j_scripts[i] = jit_compile_script(scripts[i]);
			});
		}

		// Yield while waiting, rather than block: a worker may still need the main thread (such as
		// for a file write), and the page stays responsive.
		while (!worker_pool->idle())
			emscripten_sleep(1);
	}
	else
	{
		for (size_t i = 0; i < scripts.size(); i++)
			j_scripts[i] = jit_compile_script(scripts[i]);
	}
	jit_flush_worker_logs();

	std::vector<JittedFunction*> fns;
	for (auto j_script : j_scripts)
	{
		if (j_script)
		{
			for (auto& j_fn : j_script->pending_compiled_jit_functions)
				fns.push_back(&j_fn);
		}
	}

	if (auto batch = start_browser_compiles(fns))
	{
		while (!poll_browser_compile(*batch))
			emscripten_sleep(1);
	}

	for (size_t i = 0; i < scripts.size(); i++)
	{
		if (j_scripts[i])
			finish_browser_compiles(j_scripts[i]->pending_compiled_jit_functions);
		on_compiled(scripts[i], j_scripts[i]);
	}
	return true;
#endif
}

#ifdef __EMSCRIPTEN__
extern "C" int em_do_commands(JittedScriptInstance* j_instance, int pc, int len, int sp)
{
	return run_script_jit_sequence(j_instance, pc, sp, len);
}

// The entry of a direct-called function (see compile_callfunc_direct): pushes the return pc the
// call site stored in ctx->call_pc, as the driver does for a call it makes.
extern "C" void em_direct_enter(JittedExecutionContext* ctx, int callee_start_pc)
{
	// Error traces read ctx->pc, which the driver sets to the call target on every call.
	ctx->pc = callee_start_pc;
	retstack_push(ctx->call_pc);
}

// The return of a direct-called function (the driver pops for one it entered).
extern "C" void em_direct_retstack_pop()
{
	retstack_pop();
}

extern "C" int em_get_register(int r)
{
	return get_register(r);
}

extern "C" void em_set_register(int r, int value)
{
	set_register(r, value);
}

extern "C" void em_set_guarded_register(int32_t arg, int32_t value, pc_t pc)
{
	ri->pc = pc;
	do_set(arg, value);
}

extern "C" void em_runtime_script_debug(int pc, int sp)
{
	extern ScriptDebugHandle* runtime_script_debug_handle;
	ri->pc = pc;
	ri->sp = sp;
	if (runtime_script_debug_handle)
		runtime_script_debug_handle->pre_command();
}

extern "C" void em_log_error(int code)
{
	if (code == 0)
		scripting_log_error_with_context("Attempted to divide by zero!");
	if (code == 1)
		scripting_log_error_with_context("Attempted to modulo by zero!");
}

extern "C" int em_pod_read(int arrayptr, int index, int pc, int no_neg)
{
	return jit_pod_read(arrayptr, index, pc, no_neg);
}

extern "C" void em_pod_write(int arrayptr, int index, int value, int type, int pc, int no_neg)
{
	jit_pod_write(arrayptr, index, value, type, pc, no_neg);
}

extern "C" int em_allocatemem(int size, int object_type, int uid)
{
	return jit_allocatemem(size, object_type, uid);
}

extern "C" void em_writepodarr(int id, int pc)
{
	jit_writepodarr(id, pc);
}

extern "C" int em_class_read(int id, int index, int pc)
{
	return jit_class_read(id, index, pc);
}

extern "C" void em_class_write(int id, int index, int value, int pc)
{
	jit_class_write(id, index, value, pc);
}

extern "C" int em_string_compare(int arrayptr_a, int arrayptr_b, int insensitive, int pc)
{
	return jit_string_compare(arrayptr_a, arrayptr_b, insensitive, pc);
}

extern "C" void em_ref_change(int is_this, int offset, int inc, int pc)
{
	jit_ref_change(is_this, offset, inc, pc);
}

extern "C" void em_mark_type_stack(int offset, int is_object)
{
	jit_mark_type_stack(offset, is_object);
}

extern "C" int em_arraysize(int arrayptr, int pc)
{
	return jit_arraysize(arrayptr, pc);
}

#endif

