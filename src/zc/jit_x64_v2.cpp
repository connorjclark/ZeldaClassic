#include "asmjit/core/func.h"
#include "asmjit/x86/x86operand.h"
#include "base/general.h"
#include "base/qrs.h"
#include "base/zdefs.h"
#include "zc/jit.h"
#include "zc/ffscript.h"
#include "zc/script_debug.h"
#include "zc/zasm_optimize.h"
#include "zc/zasm_utils.h"
#include "zc/zelda.h"
#include "zconsole/ConsoleLogger.h"
#include <fmt/format.h>
#include <memory>
#include <chrono>
#include <optional>
#include <asmjit/asmjit.h>

using namespace asmjit;

typedef int32_t (*JittedFunctionImpl)(int32_t *registers, int32_t *global_registers,
								  int32_t *stack, uint32_t *stack_index, uint32_t *pc,
								  intptr_t *call_stack_rets, uint32_t *call_stack_ret_index,
								  uint32_t *wait_index);

typedef void (*JittedFunctionImpl2)();

struct JittedFunctionHandle
{
	JittedFunctionImpl fn;
	bool by_block;
	std::optional<StructuredZasm> structured_zasm;
	std::optional<ZasmCFG> cfg;
	std::vector<pc_t> block_starts;
	std::map<pc_t, JittedFunctionImpl2> codes;
};

struct JittedScriptHandle
{
	JittedFunctionHandle* fn_handle;
	script_data *script;
	refInfo *ri;
	intptr_t call_stack_rets[100];
	uint32_t call_stack_ret_index;
	bool by_block;
};

static JitRuntime rt;
static JittedScriptHandle* g_jitted_script;
static int g_ret_code;
static int32_t* g_registers;
static int32_t* g_stack_base;
static int g_sp;
static int g_wait_index;
static int g_pc;

struct CompilationState
{
	CallConvId calling_convention;
	// Some globals to prevent passing around everywhere
	size_t size;
	x86::Gp vRetVal;
	// TODO ! rename vSp
	x86::Gp vStackIndex;
	x86::Gp vSwitchKey;
	Label L_End;
	// Registers for the compiled function parameters.
	x86::Gp ptrRegisters;
	x86::Gp ptrGlobalRegisters;
	x86::Gp ptrStack;
	x86::Gp ptrStackIndex;
	// x86::Gp ptrPc; // TODO !
	x86::Gp ptrCallStackRets;
	x86::Gp ptrCallStackRetIndex;
	x86::Gp ptrWaitIndex;
};

extern ScriptDebugHandle* runtime_script_debug_handle;

static void debug_pre_command(int32_t pc, uint16_t sp)
{
	extern refInfo *ri;

	ri->pc = pc;
	ri->sp = sp;
	if (runtime_script_debug_handle)
		runtime_script_debug_handle->pre_command();
}

class MyErrorHandler : public ErrorHandler
{
public:
	void handleError(Error err, const char *message, BaseEmitter *origin) override
	{
		al_trace("AsmJit error: %s\n", message);
	}
};

static x86::Gp get_z_register(CompilationState& state, x86::Compiler &cc, int r)
{
	x86::Gp val = cc.newInt32();
	if (r >= D(0) && r <= A(1))
	{
		cc.mov(val, x86::ptr_32(state.ptrRegisters, r * 4));
	}
	else if (r >= GD(0) && r <= GD(MAX_SCRIPT_REGISTERS))
	{
		cc.mov(val, x86::ptr_32(state.ptrGlobalRegisters, (r - GD(0)) * 4));
	}
	else if (r == SP)
	{
		cc.mov(val, state.vStackIndex);
		cc.imul(val, 10000);
	}
	else if (r == SP2)
	{
		cc.mov(val, state.vStackIndex);
	}
	else if (r == SWITCHKEY)
	{
		cc.mov(val, state.vSwitchKey);
	}
	else
	{
		// Call external get_register.
		InvokeNode *invokeNode;
		cc.invoke(&invokeNode, get_register, FuncSignatureT<int32_t, int32_t>(state.calling_convention));
		invokeNode->setArg(0, r);
		invokeNode->setRet(0, val);
	}
	return val;
}

static x86::Gp get_z_register_64(CompilationState& state, x86::Compiler &cc, int r)
{
	x86::Gp val = cc.newInt64();
	if (r >= D(0) && r <= A(1))
	{
		cc.movsxd(val, x86::ptr_32(state.ptrRegisters, r * 4));
	}
	else if (r >= GD(0) && r <= GD(MAX_SCRIPT_REGISTERS))
	{
		cc.movsxd(val, x86::ptr_32(state.ptrGlobalRegisters, (r - GD(0)) * 4));
	}
	else if (r == SP)
	{
		cc.movsxd(val, state.vStackIndex);
		cc.imul(val, 10000);
	}
	else if (r == SP2)
	{
		cc.movsxd(val, state.vStackIndex);
	}
	else if (r == SWITCHKEY)
	{
		cc.movsxd(val, state.vSwitchKey);
	}
	else
	{
		// Call external get_register.
		x86::Gp val32 = cc.newInt32();
		InvokeNode *invokeNode;
		cc.invoke(&invokeNode, get_register, FuncSignatureT<int32_t, int32_t>(state.calling_convention));
		invokeNode->setArg(0, r);
		invokeNode->setRet(0, val32);
		cc.movsxd(val, val32);
	}
	return val;
}

template <typename T>
static void set_z_register(CompilationState& state, x86::Compiler &cc, int r, T val)
{
	if (r >= D(0) && r <= A(1))
	{
		cc.mov(x86::ptr_32(state.ptrRegisters, r * 4), val);
	}
	else if (r >= GD(0) && r <= GD(MAX_SCRIPT_REGISTERS))
	{
		cc.mov(x86::ptr_32(state.ptrGlobalRegisters, (r - GD(0)) * 4), val);
	}
	else if (r == SP || r == SP2)
	{
		// TODO
		Z_error_fatal("Unimplemented: set SP");
	}
	else if (r == SWITCHKEY)
	{
		state.vSwitchKey = cc.newInt32();
		cc.mov(state.vSwitchKey, val);
	}
	else
	{
		// Call external set_register.
		InvokeNode *invokeNode;
		cc.invoke(&invokeNode, set_register, FuncSignatureT<void, int32_t, int32_t>(state.calling_convention));
		invokeNode->setArg(0, r);
		invokeNode->setArg(1, val);
	}
}

static void set_z_register(CompilationState& state, x86::Compiler &cc, int r, x86::Mem mem)
{
	x86::Gp val = cc.newInt32();
	cc.mov(val, mem);
	set_z_register(state, cc, r, val);
}

static void modify_sp(x86::Compiler &cc, x86::Gp vStackIndex, int delta)
{
	cc.add(vStackIndex, delta);
	cc.and_(vStackIndex, MASK_SP);
}

static void div_10000(x86::Compiler &cc, x86::Gp dividend)
{
	// Perform division by invariant multiplication.
	// https://clang.godbolt.org/z/c4qG3s9nW
	if (dividend.isType(RegType::kGp64))
	{
		x86::Gp input = cc.newInt64();
		cc.mov(input, dividend);

		x86::Gp r = cc.newInt64();
		cc.movabs(r, 3777893186295716171);
		cc.imul(r, r, dividend);
		cc.sar(r, 11);

		x86::Gp b = cc.newInt64();
		cc.mov(b, input);
		cc.sar(b, 63);
		cc.sub(r, b);

		cc.mov(dividend, r);
	}
	else if (dividend.isType(RegType::kGp32))
	{
		x86::Gp r = cc.newInt64();
		cc.movsxd(r, dividend);
		cc.sar(dividend, 31);
		cc.imul(r, r, 1759218605);
		cc.sar(r, 44);

		cc.sub(r.r32(), dividend);
		cc.mov(dividend, r.r32());
	}
	else
	{
		abort();
	}
}

static void zero(x86::Compiler &cc, x86::Gp reg)
{
	cc.xor_(reg, reg);
}

static void cast_bool(x86::Compiler &cc, x86::Gp reg)
{
	cc.test(reg, reg);
	cc.mov(reg, 0);
	cc.setne(reg.r8());
}

template <typename T>
static void compile_set_global(x86::Compiler &cc, void* ptr, T value)
{
	x86::Gp reg = cc.newIntPtr();
	cc.mov(reg, ptr);
	cc.mov(x86::ptr_32(reg), value);
}

static void compile_get_global(x86::Compiler &cc, void* ptr, x86::Gp& out)
{
	x86::Gp reg = cc.newIntPtr();
	cc.mov(reg, ptr);
	cc.mov(out, x86::ptr_32(reg));
}

static void compile_compare(CompilationState& state, x86::Compiler &cc, int pc, int command, int arg1, int arg2, int arg3)
{
	x86::Gp val = cc.newInt32();
	
	if(command == GOTOCMP)
	{
		// auto lbl = goto_labels.at(arg1);
		// switch(arg2 & CMP_FLAGS)
		// {
		// 	default:
		// 		break;
		// 	case CMP_GT:
		// 		cc.jg(lbl);
		// 		break;
		// 	case CMP_GT|CMP_EQ:
		// 		cc.jge(lbl);
		// 		break;
		// 	case CMP_LT:
		// 		cc.jl(lbl);
		// 		break;
		// 	case CMP_LT|CMP_EQ:
		// 		cc.jle(lbl);
		// 		break;
		// 	case CMP_EQ:
		// 		cc.je(lbl);
		// 		break;
		// 	case CMP_GT|CMP_LT:
		// 		cc.jne(lbl);
		// 		break;
		// 	case CMP_GT|CMP_LT|CMP_EQ:
		// 		cc.jmp(lbl);
		// 		break;
		// }

		cc.mov(val, pc + 1);
		switch(arg2 & CMP_FLAGS)
		{
			default:
				break;
			case CMP_GT:
				cc.setg(val);
				break;
			case CMP_GT|CMP_EQ:
				cc.setge(val);
				break;
			case CMP_LT:
				cc.setl(val);
				break;
			case CMP_LT|CMP_EQ:
				cc.setle(val);
				break;
			case CMP_EQ:
				cc.sete(val);
				break;
			case CMP_GT|CMP_LT:
				cc.setne(val);
				break;
			case CMP_GT|CMP_LT|CMP_EQ:
				cc.mov(val, 1);
				break;
		}

		compile_set_global(cc, &g_pc, val);
	}
	else if (command == SETCMP)
	{
		cc.mov(val, 0);
		bool i10k = (arg2 & CMP_SETI);
		x86::Gp val2;
		if(i10k)
		{
			val2 = cc.newInt32();
			cc.mov(val2, 10000);
		}
		switch(arg2 & CMP_FLAGS)
		{
			default:
				break;
			case CMP_GT:
				if(i10k)
					cc.cmovg(val, val2);
				else cc.setg(val);
				break;
			case CMP_GT|CMP_EQ:
				if(i10k)
					cc.cmovge(val, val2);
				else cc.setge(val);
				break;
			case CMP_LT:
				if(i10k)
					cc.cmovl(val, val2);
				else cc.setl(val);
				break;
			case CMP_LT|CMP_EQ:
				if(i10k)
					cc.cmovle(val, val2);
				else cc.setle(val);
				break;
			case CMP_EQ:
				if(i10k)
					cc.cmove(val, val2);
				else cc.sete(val);
				break;
			case CMP_GT|CMP_LT:
				if(i10k)
					cc.cmovne(val, val2);
				else cc.setne(val);
				break;
			case CMP_GT|CMP_LT|CMP_EQ:
				if(i10k)
					cc.mov(val, 10000);
				else cc.mov(val, 1);
				break;
		}
		set_z_register(state, cc, arg1, val);
	}
	else if (command == GOTOTRUE)
	{
		ASSERT(false); // TODO !
		// cc.je(goto_labels.at(arg1));
	}
	else if (command == GOTOFALSE)
	{
		ASSERT(false); // TODO !
		// cc.jne(goto_labels.at(arg1));
	}
	else if (command == GOTOMORE)
	{
		ASSERT(false); // TODO !
		// cc.jge(goto_labels.at(arg1));
	}
	else if (command == GOTOLESS)
	{
		ASSERT(false); // TODO !
		// if (get_qr(qr_GOTOLESSNOTEQUAL))
		// 	cc.jle(goto_labels.at(arg1));
		// else
		// 	cc.jl(goto_labels.at(arg1));
	}
	else if (command == SETTRUE)
	{
		cc.mov(val, 0);
		cc.sete(val);
		set_z_register(state, cc, arg1, val);
	}
	else if (command == SETTRUEI)
	{
		// https://stackoverflow.com/a/45183084/2788187
		cc.mov(val, 0);
		x86::Gp val2 = cc.newInt32();
		cc.mov(val2, 10000);
		cc.cmove(val, val2);
		set_z_register(state, cc, arg1, val);
	}
	else if (command == SETFALSE)
	{
		cc.mov(val, 0);
		cc.setne(val);
		set_z_register(state, cc, arg1, val);
	}
	else if (command == SETFALSEI)
	{
		cc.mov(val, 0);
		x86::Gp val2 = cc.newInt32();
		cc.mov(val2, 10000);
		cc.cmovne(val, val2);
		set_z_register(state, cc, arg1, val);
	}
	else if (command == SETMOREI)
	{
		cc.mov(val, 0);
		x86::Gp val2 = cc.newInt32();
		cc.mov(val2, 10000);
		cc.cmovge(val, val2);
		set_z_register(state, cc, arg1, val);
	}
	else if (command == SETLESSI)
	{
		cc.mov(val, 0);
		x86::Gp val2 = cc.newInt32();
		cc.mov(val2, 10000);
		cc.cmovle(val, val2);
		set_z_register(state, cc, arg1, val);
	}
	else if (command == SETMORE)
	{
		cc.mov(val, 0);
		cc.setge(val);
		set_z_register(state, cc, arg1, val);
	}
	else if (command == SETLESS)
	{
		cc.mov(val, 0);
		cc.setle(val);
		set_z_register(state, cc, arg1, val);
	}
	else if(command == STACKWRITEATVV_IF)
	{
		// Write directly value on the stack (arg1 to offset arg2)
		x86::Gp offset = cc.newInt32();
		ASSERT(false); // cc.mov(offset, vStackIndex); TODO !
		if (arg2)
			cc.add(offset, arg2);
		auto cmp = arg3 & CMP_FLAGS;
		switch(cmp) //but only conditionally
		{
			case 0:
				break;
			case CMP_GT|CMP_LT|CMP_EQ:
				cc.mov(x86::ptr_32(state.ptrStack, offset, 2), arg1);
				break;
			default:
			{
				x86::Gp tmp = cc.newInt32();
				x86::Gp val = cc.newInt32();
				cc.mov(tmp, x86::ptr_32(state.ptrStack, offset, 2));
				cc.mov(val, arg1);
				switch(cmp)
				{
					case CMP_GT:
						cc.cmovg(tmp, val);
						break;
					case CMP_GT|CMP_EQ:
						cc.cmovge(tmp, val);
						break;
					case CMP_LT:
						cc.cmovl(tmp, val);
						break;
					case CMP_LT|CMP_EQ:
						cc.cmovle(tmp, val);
						break;
					case CMP_EQ:
						cc.cmove(tmp, val);
						break;
					case CMP_GT|CMP_LT:
						cc.cmovne(tmp, val);
						break;
					default:
						assert(false);
				}
				cc.mov(x86::ptr_32(state.ptrStack, offset, 2), tmp);
			}
		}
	}
	else
	{
		Z_error_fatal("Unimplemented: %s", script_debug_command_to_string(command, arg1, arg2, arg3).c_str());
	}
}

// Defer to the ZASM command interpreter for 1+ commands.
static void compile_command_interpreter(CompilationState& state, x86::Compiler &cc, script_data *script, int i, int count, bool is_wait = false)
{
	// extern int32_t jitted_uncompiled_command_count;
	extern refInfo *ri;

	// x86::Gp reg = cc.newIntPtr();
	// cc.mov(reg, (uint64_t)&jitted_uncompiled_command_count);
	// cc.mov(x86::ptr_32(reg), count);

	// cc.mov(x86::ptr_32(state.ptrStackIndex), vStackIndex);

	InvokeNode *invokeNode;
	cc.invoke(&invokeNode, run_script_jit_entry, FuncSignatureT<int32_t, int32_t, int32_t>(state.calling_convention));
	invokeNode->setArg(0, i);
	invokeNode->setArg(1, count);

	if (is_wait)
	{
		x86::Gp retVal = cc.newInt32();
		invokeNode->setRet(0, retVal);
		cc.cmp(retVal, RUNSCRIPT_OK);
		cc.jne(state.L_End);
		return;
	}

	bool could_return_not_ok = false;
	for (int j = 0; j < count; j++)
	{
		int index = i + j;
		if (index >= state.size)
			break;

		if (command_could_return_not_ok(script->zasm[index].command))
		{
			could_return_not_ok = true;
			break;
		}
	}

	if (could_return_not_ok)
	{
		Label L_noret = cc.newLabel();

		x86::Gp retVal = cc.newInt32();
		invokeNode->setRet(0, retVal);
		cc.cmp(retVal, RUNSCRIPT_OK);
		cc.je(L_noret);

		compile_set_global(cc, &g_sp, state.vStackIndex);
		compile_set_global(cc, &g_ret_code, retVal);
		cc.ret();

		cc.bind(L_noret);
	}
}

static bool command_is_compiled(int command)
{
	if (command_is_wait(command))
		return true;

	if (command_uses_comparison_result(command))
		return true;

	switch (command)
	{
	// These commands are critical to control flow.
	case COMPARER:
	case COMPAREV:
	case COMPAREV2:
	case GOTO:
	case GOTOR:
	case QUIT:
	case RETURN:
	case CALLFUNC:
	case RETURNFUNC:

	// These commands modify the stack pointer, which is just a local copy. If these commands
	// were not compiled, then vStackIndex would have to be restored after compile_command_interpreter.
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
	case PEEK:
	case SETR:
	case SETV:
	case STORE:
	case STORED:
	case STOREDV:
	case STOREI:
	case STOREV:
	case SUBR:
	case SUBV:
	case SUBV2:
	
	//
	case STACKWRITEATVV:
		return true;
	}

	return false;
}

static void error(ScriptDebugHandle* debug_handle, script_data *script, std::string str)
{
	str = fmt::format("failed to compile type: {} index: {} name: {}\nerror: {}\n",
					  ScriptTypeToString(script->id.type), script->id.index, script->meta.script_name.c_str(), str);

	al_trace("%s", str.c_str());
	if (debug_handle)
		debug_handle->print(CConsoleLoggerEx::COLOR_RED | CConsoleLoggerEx::COLOR_INTENSITY | CConsoleLoggerEx::COLOR_BACKGROUND_BLACK, str.c_str());

	if (DEBUG_JIT_EXIT_ON_COMPILE_FAIL)
	{
		abort();
	}
}

// Useful if crashing at runtime to find the last command that ran.
// #define JIT_DEBUG_CRASH
#ifdef JIT_DEBUG_CRASH
static size_t debug_last_pc;
#endif

// Every command here must be reflected in command_is_compiled!
static void compile_single_command(CompilationState& state, x86::Compiler &cc, const ffscript& instr, int i, script_data *script)
{
	int command = instr.command;
	int arg1 = instr.arg1;
	int arg2 = instr.arg2;

	switch (command)
	{
		case NOP:
			if (DEBUG_JIT_PRINT_ASM)
				cc.nop();
			break;
		case QUIT:
		{
			compile_command_interpreter(state, cc, script, i, 1);
			cc.mov(state.vRetVal, RUNSCRIPT_STOPPED);
			cc.mov(x86::ptr_32(state.ptrWaitIndex), 0);
			cc.jmp(state.L_End);
		}
		break;
		// TODO !
		// case CALLFUNC:
		// 	//Normally pushes a return address to the 'ret_stack'
		// 	//...but we can ignore that when jitted
		// [[fallthrough]];
		// case GOTO:
		// {
		// 	if (structured_zasm.function_calls.contains(i))
		// 	{
		// 		// https://github.com/asmjit/asmjit/issues/286
		// 		x86::Gp address = cc.newIntPtr();
		// 		cc.mov(x86::qword_ptr(state.ptrCallStackRets, vCallStackRetIndex, 3), i);
		// 		cc.add(vCallStackRetIndex, 1);
		// 		compile_set_global(cc, &g_pc, arg1);
		// 		cc.bind(call_pc_to_return_label.at(i));
		// 	}
		// 	else
		// 	{
		// 		compile_set_global(cc, &g_pc, arg1);
		// 	}
		// }
		// break;

		// // GOTOR is pretty much RETURN - was only used to return to the call location in scripts
		// // compiled before RETURN existed.
		// // Note: for GOTOR the return pc is in a register, but we just ignore it and instead use
		// // the function call return label.
		// case GOTOR:
		// case RETURN:
		// {
		// 	// Note: for RETURN the return pc is on the stack, but we just ignore it and instead use
		// 	// the function call return label.
		// 	if (command == RETURN)
		// 		modify_sp(cc, state.vStackIndex, 1);

		// 	cc.sub(vCallStackRetIndex, 1);
		// 	x86::Gp address = cc.newIntPtr();
		// 	cc.mov(address, x86::qword_ptr(state.ptrCallStackRets, vCallStackRetIndex, 3));

		// 	int function_index = return_to_function_id.at(i);
		// 	if (function_jump_annotations.size() <= function_index)
		// 	{
		// 		error(debug_handle, script, fmt::format("failed to resolve function return! i: {} function_index: {}", i, function_index));
		// 		return nullptr;
		// 	}
		// 	cc.jmp(address, function_jump_annotations[function_index]);
		// }
		// break;
		// case RETURNFUNC:
		// {
		// 	//Normally the return address is on the 'ret_stack'
		// 	//...but we can ignore that when jitted

		// 	cc.sub(vCallStackRetIndex, 1);
		// 	x86::Gp address = cc.newIntPtr();
		// 	cc.mov(address, x86::qword_ptr(state.ptrCallStackRets, vCallStackRetIndex, 3));

		// 	int function_index = return_to_function_id.at(i);
		// 	if (function_jump_annotations.size() <= function_index)
		// 	{
		// 		error(debug_handle, script, fmt::format("failed to resolve function return! i: {} function_index: {}", i, function_index));
		// 		return nullptr;
		// 	}
		// 	cc.jmp(address, function_jump_annotations[function_index]);
		// }
		// break;
		case STACKWRITEATVV:
		{
			// Write directly value on the stack (arg1 to offset arg2)
			x86::Gp offset = cc.newInt32();
			cc.mov(offset, state.vStackIndex);
			if (arg2)
				cc.add(offset, arg2);
			cc.mov(x86::ptr_32(state.ptrStack, offset, 2), arg1);
		}
		break;
		case PUSHV:
		{
			modify_sp(cc, state.vStackIndex, -1);
			cc.mov(x86::ptr_32(state.ptrStack, state.vStackIndex, 2), arg1);
		}
		break;
		case PUSHR:
		{
			// Grab value from register and push onto stack.
			x86::Gp val = get_z_register(state, cc, arg1);
			modify_sp(cc, state.vStackIndex, -1);
			cc.mov(x86::ptr_32(state.ptrStack, state.vStackIndex, 2), val);
		}
		break;
		case PUSHARGSR:
		{
			if(arg2 < 1) break; //do nothing
			// Grab value from register and push onto stack, repeatedly
			x86::Gp val = get_z_register(state, cc, arg1);
			for(int q = 0; q < arg2; ++q)
			{
				modify_sp(cc, state.vStackIndex, -1);
				cc.mov(x86::ptr_32(state.ptrStack, state.vStackIndex, 2), val);
			}
		}
		break;
		case PUSHARGSV:
		{
			if(arg2 < 1) break; //do nothing
			// Push value onto stack, repeatedly
			for(int q = 0; q < arg2; ++q)
			{
				modify_sp(cc, state.vStackIndex, -1);
				cc.mov(x86::ptr_32(state.ptrStack, state.vStackIndex, 2), arg1);
			}
		}
		break;
		case SETV:
		{
			// Set register to immediate value.
			set_z_register(state, cc, arg1, arg2);
		}
		break;
		case SETR:
		{
			// Set register arg1 to value of register arg2.
			x86::Gp val = get_z_register(state, cc, arg2);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case LOAD:
		{
			// Set register to a value on the stack (offset is arg2 + rSFRAME register).
			x86::Gp offset = cc.newInt32();
			cc.mov(offset, x86::ptr_32(state.ptrRegisters, rSFRAME * 4));
			if (arg2)
				cc.add(offset, arg2);

			set_z_register(state, cc, arg1, x86::ptr_32(state.ptrStack, offset, 2));
		}
		break;
		case LOADD:
		{
			// Set register to a value on the stack (offset is arg2 + rSFRAME register).
			x86::Gp offset = cc.newInt32();
			cc.mov(offset, x86::ptr_32(state.ptrRegisters, rSFRAME * 4));
			if (arg2)
				cc.add(offset, arg2);
			div_10000(cc, offset);

			set_z_register(state, cc, arg1, x86::ptr_32(state.ptrStack, offset, 2));
		}
		break;
		case LOADI:
		{
			// Set register to a value on the stack (offset is register at arg2).
			x86::Gp offset = get_z_register(state, cc, arg2);
			div_10000(cc, offset);

			set_z_register(state, cc, arg1, x86::ptr_32(state.ptrStack, offset, 2));
		}
		break;
		case STORE:
		{
			// Write from register to a value on the stack (offset is arg2 + rSFRAME register).
			x86::Gp offset = cc.newInt32();
			cc.mov(offset, arg2);
			cc.add(offset, x86::ptr_32(state.ptrRegisters, rSFRAME * 4));

			x86::Gp val = get_z_register(state, cc, arg1);
			cc.mov(x86::ptr_32(state.ptrStack, offset, 2), val);
		}
		break;
		case STOREV:
		{
			// Write directly value on the stack (offset is arg2 + rSFRAME register).
			x86::Gp offset = cc.newInt32();
			cc.mov(offset, x86::ptr_32(state.ptrRegisters, rSFRAME * 4));
			if (arg2)
				cc.add(offset, arg2);
			
			cc.mov(x86::ptr_32(state.ptrStack, offset, 2), arg1);
		}
		break;
		case STORED:
		{
			// Write from register to a value on the stack (offset is arg2 + rSFRAME register).
			x86::Gp offset = cc.newInt32();
			cc.mov(offset, arg2);
			cc.add(offset, x86::ptr_32(state.ptrRegisters, rSFRAME * 4));
			div_10000(cc, offset);
			
			x86::Gp val = get_z_register(state, cc, arg1);
			cc.mov(x86::ptr_32(state.ptrStack, offset, 2), val);
		}
		break;
		case STOREDV:
		{
			// Write directly value on the stack (offset is arg2 + rSFRAME register).
			x86::Gp offset = cc.newInt32();
			cc.mov(offset, x86::ptr_32(state.ptrRegisters, rSFRAME * 4));
			if (arg2)
				cc.add(offset, arg2);
			div_10000(cc, offset);
			
			cc.mov(x86::ptr_32(state.ptrStack, offset, 2), arg1);
		}
		break;
		case STOREI:
		{
			// Write from register to a value on the stack (offset is register at arg2).
			x86::Gp offset = get_z_register(state, cc, arg2);
			div_10000(cc, offset);

			x86::Gp val = get_z_register(state, cc, arg1);
			cc.mov(x86::ptr_32(state.ptrStack, offset, 2), val);
		}
		break;
		case POP:
		{
			x86::Gp val = cc.newInt32();
			cc.mov(val, x86::ptr_32(state.ptrStack, state.vStackIndex, 2));
			modify_sp(cc, state.vStackIndex, 1);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case POPARGS:
		{
			// int32_t num = sarg2;
			// ri->sp += num;
			modify_sp(cc, state.vStackIndex, arg2);

			// word read = (ri->sp-1) & MASK_SP;
			x86::Gp read = cc.newInt32();
			cc.mov(read, state.vStackIndex);
			cc.sub(read, 1);
			cc.and_(read, MASK_SP);

			// int32_t value = SH::read_stack(read);
			// set_register(sarg1, value);
			x86::Gp val = cc.newInt32();
			cc.mov(val, x86::ptr_32(state.ptrStack, read, 2));
			set_z_register(state, cc, arg1, val);
		}
		break;
		case ABS:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			x86::Gp y = cc.newInt32();
			cc.mov(y, val);
			cc.sar(y, 31);
			cc.xor_(val, y);
			cc.sub(val, y);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case CASTBOOLI:
		{
			// https://clang.godbolt.org/z/W8PM4j33b
			x86::Gp val = get_z_register(state, cc, arg1);
			cc.neg(val);
			cc.sbb(val, val);
			cc.and_(val, 10000);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case CASTBOOLF:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			cast_bool(cc, val);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case ADDV:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			cc.add(val, arg2);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case ADDR:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			x86::Gp val2 = get_z_register(state, cc, arg2);
			cc.add(val, val2);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case ANDV:
		{
			x86::Gp val = get_z_register(state, cc, arg1);

			div_10000(cc, val);
			cc.and_(val, arg2 / 10000);
			cc.imul(val, 10000);

			set_z_register(state, cc, arg1, val);
		}
		break;
		case ANDR:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			x86::Gp val2 = get_z_register(state, cc, arg2);

			div_10000(cc, val);
			div_10000(cc, val2);
			cc.and_(val, val2);
			cc.imul(val, 10000);

			set_z_register(state, cc, arg1, val);
		}
		break;
		case MAXR:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			x86::Gp val2 = get_z_register(state, cc, arg2);
			cc.cmp(val2, val);
			cc.cmovge(val, val2);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case MAXV:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			x86::Gp val2 = cc.newInt32();
			cc.mov(val2, arg2);
			cc.cmp(val2, val);
			cc.cmovge(val, val2);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case MINR:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			x86::Gp val2 = get_z_register(state, cc, arg2);
			cc.cmp(val, val2);
			cc.cmovge(val, val2);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case MINV:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			x86::Gp val2 = cc.newInt32();
			cc.mov(val2, arg2);
			cc.cmp(val, val2);
			cc.cmovge(val, val2);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case MODV:
		{
			if (arg2 == 0)
			{
				x86::Gp val = cc.newInt32();
				zero(cc, val);
				set_z_register(state, cc, arg1, val);
				return;
			}

			// https://stackoverflow.com/a/8022107/2788187
			x86::Gp val = get_z_register(state, cc, arg1);
			if (arg2 > 0 && (arg2 & (-arg2)) == arg2)
			{
				// Power of 2.
				// Because numbers in zscript are fixed point, "2" is really "20000"... so this won't
				// ever really be utilized.
				cc.and_(val, arg2 - 1);
				set_z_register(state, cc, arg1, val);
			}
			else
			{
				x86::Gp divisor = cc.newInt32();
				cc.mov(divisor, arg2);
				x86::Gp rem = cc.newInt32();
				zero(cc, rem);
				cc.cdq(rem, val);
				cc.idiv(rem, val, divisor);
				set_z_register(state, cc, arg1, rem);
			}
		}
		break;
		case MODR:
		{
			x86::Gp dividend = get_z_register(state, cc, arg1);
			x86::Gp divisor = get_z_register(state, cc, arg2);

			Label do_set_register = cc.newLabel();

			x86::Gp rem = cc.newInt32();
			zero(cc, rem);

			// Prevent division by zero. Result will be zero.
			cc.test(divisor, divisor);
			cc.jz(do_set_register);

			cc.cdq(rem, dividend);
			cc.idiv(rem, dividend, divisor);

			cc.bind(do_set_register);
			set_z_register(state, cc, arg1, rem);
		}
		break;
		case SUBV:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			cc.sub(val, arg2);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case SUBR:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			x86::Gp val2 = get_z_register(state, cc, arg2);
			cc.sub(val, val2);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case SUBV2:
		{
			x86::Gp val = get_z_register(state, cc, arg2);
			x86::Gp result = cc.newInt32();
			cc.mov(result, arg1);
			cc.sub(result, val);
			set_z_register(state, cc, arg2, result);
		}
		break;
		case MULTV:
		{
			x86::Gp val = get_z_register_64(state, cc, arg1);
			cc.imul(val, arg2);
			div_10000(cc, val);
			set_z_register(state, cc, arg1, val.r32());
		}
		break;
		case MULTR:
		{
			x86::Gp val = get_z_register_64(state, cc, arg1);
			x86::Gp val2 = get_z_register_64(state, cc, arg2);
			cc.imul(val, val2);
			div_10000(cc, val);
			set_z_register(state, cc, arg1, val.r32());
		}
		break;
		// TODO guard for div by zero
		case DIVV:
		{
			x86::Gp dividend = get_z_register_64(state, cc, arg1);
			int val2 = arg2;

			cc.imul(dividend, 10000);
			x86::Gp divisor = cc.newInt64();
			cc.mov(divisor, val2);
			x86::Gp dummy = cc.newInt64();
			zero(cc, dummy);
			cc.cqo(dummy, dividend);
			cc.idiv(dummy, dividend, divisor);

			set_z_register(state, cc, arg1, dividend.r32());
		}
		break;
		case DIVR:
		{
			x86::Gp dividend = get_z_register_64(state, cc, arg1);
			x86::Gp divisor = get_z_register_64(state, cc, arg2);

			Label do_division = cc.newLabel();
			Label do_set_register = cc.newLabel();

			// If zero, result is (sign(dividend) * MAX_SIGNED_32)
			// TODO how expensive is this check? Maybe we can add a new QR that makes div-by-zero an error
			// and omit these safeguards.
			cc.test(divisor, divisor);
			cc.jnz(do_division);
			x86::Gp sign = cc.newInt64();
			cc.mov(sign, dividend);
			cc.sar(sign, 63);
			cc.or_(sign, 1);
			cc.mov(dividend.r32(), sign.r32());
			cc.imul(dividend.r32(), MAX_SIGNED_32);
			cc.jmp(do_set_register);

			// Else do the actual division.
			cc.bind(do_division);
			cc.imul(dividend, 10000);
			x86::Gp dummy = cc.newInt64();
			zero(cc, dummy);
			cc.cqo(dummy, dividend);
			cc.idiv(dummy, dividend, divisor);

			cc.bind(do_set_register);
			set_z_register(state, cc, arg1, dividend.r32());
		}
		break;
		case COMPAREV:
		{
			int val = arg2;
			x86::Gp val2 = get_z_register(state, cc, arg1);

			if (script->zasm[i + 1].command == GOTOCMP || script->zasm[i + 1].command == SETCMP)
			{
				if (script->zasm[i + 1].arg2 & CMP_BOOL)
				{
					val = val ? 1 : 0;
					cast_bool(cc, val2);
				}
			}

			cc.cmp(val2, val);
		}
		break;
		case COMPAREV2:
		{
			int val = arg1;
			x86::Gp val2 = get_z_register(state, cc, arg2);

			if (script->zasm[i + 1].command == GOTOCMP || script->zasm[i + 1].command == SETCMP)
			{
				if (script->zasm[i + 1].arg2 & CMP_BOOL)
				{
					val = val ? 1 : 0;
					cast_bool(cc, val2);
				}
			}

			cc.cmp(val2, val);
		}
		break;
		case COMPARER:
		{
			x86::Gp val = get_z_register(state, cc, arg2);
			x86::Gp val2 = get_z_register(state, cc, arg1);

			if (script->zasm[i + 1].command == GOTOCMP || script->zasm[i + 1].command == SETCMP)
			{
				if (script->zasm[i + 1].arg2 & CMP_BOOL)
				{
					cast_bool(cc, val);
					cast_bool(cc, val2);
				}
			}

			cc.cmp(val2, val);
		}
		break;
		// https://gcc.godbolt.org/z/r9zq67bK1
		case FLOOR:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			x86::Xmm y = cc.newXmm();
			x86::Mem mem = cc.newQWordConst(ConstPoolScope::kGlobal, 4547007122018943789);
			cc.cvtsi2sd(y, val);
			cc.mulsd(y, mem);
			cc.roundsd(y, y, 9);
			cc.cvttsd2si(val, y);
			cc.imul(val, val, 10000);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case CEILING:
		{
			x86::Gp val = get_z_register(state, cc, arg1);
			x86::Xmm y = cc.newXmm();
			x86::Mem mem = cc.newQWordConst(ConstPoolScope::kGlobal, 4547007122018943789);
			cc.cvtsi2sd(y, val);
			cc.mulsd(y, mem);
			cc.roundsd(y, y, 10);
			cc.cvttsd2si(val, y);
			cc.imul(val, val, 10000);
			set_z_register(state, cc, arg1, val);
		}
		break;
		case PEEK:
		{
			x86::Gp val = cc.newInt32();
			cc.mov(val, x86::ptr_32(state.ptrStack, state.vStackIndex, 2));
			set_z_register(state, cc, arg1, val);
		}
		break;
		default:
		{
			// TODO !
			// error(debug_handle, script, fmt::format("unhandled command: {}", command));
			// return nullptr;
			ASSERT(false);
		}
	}
}

// Compile the entire ZASM script at once, into a single function.
JittedFunction jit_compile_script(script_data *script)
{
	if (script->size <= 1)
		return nullptr;

	// TODO !
	// if (script->id != script_id{ScriptType::Global, 0} && script->id != script_id{ScriptType::FFC, 2})
	if (script->id != script_id{ScriptType::FFC, 2})
		return nullptr;

	if (zasm_optimize_enabled() && !script->optimized)
		zasm_optimize_and_log(script);

	CompilationState state;
	state.size = script->size;
	size_t size = state.size;

	al_trace("[jit] compiling script type: %s index: %d size: %zu name: %s\n", ScriptTypeToString(script->id.type), script->id.index, size, script->meta.script_name.c_str());

	auto result = new JittedFunctionHandle{nullptr, true, zasm_construct_structured(script), std::nullopt};
	std::vector<std::pair<pc_t, pc_t>> pc_ranges;
	for (const auto& fn : result->structured_zasm->functions)
	{
		pc_ranges.emplace_back(fn.start_pc, fn.final_pc);
	}
	result->cfg = zasm_construct_cfg(script, pc_ranges);
	result->block_starts = std::vector<pc_t>(result->cfg->block_starts.begin(), result->cfg->block_starts.end());
	return result;
}

JittedScriptHandle *jit_create_script_handle_impl(script_data *script, refInfo* ri, JittedFunction fn)
{
	JittedScriptHandle *jitted_script = new JittedScriptHandle;
	jitted_script->call_stack_ret_index = 0;
	jitted_script->script = script;
	jitted_script->ri = ri;
	jitted_script->fn_handle = (JittedFunctionHandle*)fn;
	return jitted_script;
}

void jit_reinit(JittedScriptHandle *jitted_script)
{
	jitted_script->call_stack_ret_index = 0;
}

static JittedFunctionImpl2 compile_block(JittedScriptHandle *jitted_script, pc_t block_start)
{
	script_data* script = jitted_script->script;

	std::optional<ScriptDebugHandle> debug_handle_ = std::nullopt;
	if (DEBUG_JIT_PRINT_ASM)
	{
		// TODO ! append
		debug_handle_.emplace(ScriptDebugHandle::OutputSplit::ByScript, script);
	}
	auto debug_handle = debug_handle_ ? std::addressof(debug_handle_.value()) : nullptr;

	std::chrono::steady_clock::time_point start_time, end_time;
	start_time = std::chrono::steady_clock::now();

	bool runtime_debugging = script_debug_is_runtime_debugging() == 2;

	// pc_t final_pc = start_pc + 1;
	// while (final_pc < script->size)
	// {
	// 	int command = script->zasm[final_pc].command;
	// 	if (command_is_goto(command))
	// 		break;

	// 	final_pc++;
	// }
	int block = -1;
	for (int i = 0; i < jitted_script->fn_handle->block_starts.size(); i++)
	{
		if (jitted_script->fn_handle->block_starts[i] == block_start)
		{
			block = i;
			break;
		}
	}
	ASSERT(block != -1);

	pc_t block_final = jitted_script->fn_handle->block_starts.size() - 1 == block ?
		jitted_script->script->size - 1 :
		jitted_script->fn_handle->block_starts.at(block + 1) - 1;

	CompilationState state;
	state.size = script->size;
	size_t size = state.size;

	CodeHolder code;
	JittedFunctionImpl2 fn;

	static bool jit_env_test = get_flag_bool("-jit-env-test").value_or(false);
	state.calling_convention = CallConvId::kHost;
	if (jit_env_test)
	{
		// This is only for testing purposes, to ensure the same output regardless of
		// the host machine. Used by test_jit.py
		Environment env{};
		env._arch = Arch::kX64;
		env._platform = Platform::kOSX;
		env._platformABI = PlatformABI::kGNU;
		env._objectFormat = ObjectFormat::kJIT;
		code.init(env);
		state.calling_convention = CallConvId::kCDecl;
	}
	else
	{
		code.init(rt.environment());
	}

	MyErrorHandler myErrorHandler;
	code.setErrorHandler(&myErrorHandler);

	StringLogger logger;
	if (DEBUG_JIT_PRINT_ASM)
	{
		code.setLogger(&logger);
	}

	x86::Compiler cc(&code);
	cc.addFunc(FuncSignatureT<void>(state.calling_convention));
	{
		// state.vStackIndex = cc.newInt32("g_sp");
		// compile_get_global(cc, &g_sp, state.vStackIndex);
		compile_get_global(cc, &g_sp, state.vStackIndex = cc.newInt32("g_sp"));
		compile_get_global(cc, &g_registers, state.ptrRegisters = cc.newIntPtr("g_registers"));
		compile_get_global(cc, &g_stack_base, state.ptrStack = cc.newIntPtr("g_stack_base"));
	}

	// cc.setInlineComment does not make a copy of the string, so we need to keep
	// comment strings around a bit longer than the invocation.
	std::string comment;
	std::map<int, int> uncompiled_command_counts;

	for (pc_t i = block_start; i <= block_final; i++)
	{
		const auto& instr = script->zasm[i];
		int command = instr.command;
		int arg1 = instr.arg1;
		int arg2 = instr.arg2;
		int arg3 = instr.arg3;

		if (DEBUG_JIT_PRINT_ASM && jitted_script->fn_handle->structured_zasm->start_pc_to_function.contains(i))
		{
			cc.setInlineComment((comment = fmt::format("function {}", jitted_script->fn_handle->structured_zasm->start_pc_to_function.at(i))).c_str());
			cc.nop();
		}

		if (DEBUG_JIT_PRINT_ASM)
		{
			cc.setInlineComment((comment = fmt::format("{} {}", i, script_debug_command_to_string(command, arg1, arg2, arg3, instr.vecptr, instr.strptr))).c_str());
		}

		// TODO ! debug only
		compile_set_global(cc, &g_pc, i);

		if (command_uses_comparison_result(command))
		{
			compile_compare(state, cc, i, command, arg1, arg2, arg3);
			continue;
		}

		if (command_is_wait(command))
		{
			// TODO ! edit comment
			// Wait commands normally yield back to the engine, however there are some
			// special cases where it does not. For example, when WAITFRAMESR arg is 0.
			// cc.mov(x86::ptr_32(state.ptrWaitIndex), block + 1); // TODO !
			compile_set_global(cc, &g_wait_index, block + 1);
			// This will yield, but only if actually waiting.
			compile_command_interpreter(state, cc, script, i, 1, true);
			continue;
		}

		if (!command_is_compiled(command))
		{
			// TODO !
			// if (DEBUG_JIT_PRINT_ASM && command != 0xFFFF)
			// 	uncompiled_command_counts[command]++;

			if (DEBUG_JIT_PRINT_ASM)
			{
				std::string command_str =
					script_debug_command_to_string(command, arg1, arg2, arg3, instr.vecptr, instr.strptr);
				cc.setInlineComment((comment = fmt::format("{} {}", i, command_str)).c_str());
				cc.nop();
			}

			// Every command that is not compiled to assembly must go through the regular interpreter function.
			// In order to reduce function call overhead, we call into the interpreter function in batches.
			int uncompiled_command_count = 1;
			for (int j = i + 1; j <= block_final; j++)
			{
				if (command_is_compiled(script->zasm[j].command))
					break;
				// TODO ! delme
				// if (goto_labels.contains(j))
				// 	break;
				// if (structured_zasm.start_pc_to_function.contains(j))
				// 	break;

				if (DEBUG_JIT_PRINT_ASM && script->zasm[j].command != 0xFFFF)
					uncompiled_command_counts[script->zasm[j].command]++;

				uncompiled_command_count += 1;
				if (DEBUG_JIT_PRINT_ASM)
				{
					auto& op = script->zasm[j];
					std::string command_str =
						script_debug_command_to_string(op.command, op.arg1, op.arg2, op.arg3, op.vecptr, op.strptr);
					cc.setInlineComment((comment = fmt::format("{} {}", j, command_str)).c_str());
					cc.nop();
				}
			}

			compile_command_interpreter(state, cc, script, i, uncompiled_command_count);
			i += uncompiled_command_count - 1;
			continue;
		}

		compile_single_command(state, cc, instr, i, script);

		if (i == block_final)
			compile_set_global(cc, &g_pc, block_final + 1);

		// switch (instr.command)
		// {
		// 	case ADDV:
		// 	{
		// 		x86::Gp val = get_z_register(state, cc, arg1);
		// 		cc.add(val, arg2);
		// 		set_z_register(state, cc, arg1, val);
		// 	}
		// 	break;
		// }
	}

	// TODO ! handle GOTOs, set g_pc.

	compile_set_global(cc, &g_sp, state.vStackIndex);

	cc.ret();
	cc.endFunc();

	cc.finalize();
	rt.add(&fn, &code);
	ASSERT(fn);

	end_time = std::chrono::steady_clock::now();
	int32_t compile_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

	if (debug_handle)
	{
		// TODO ! rm timing?
		debug_handle->printf("time to compile:    %d ms\n", compile_ms);
		debug_handle->printf("Code size:          %d kb\n", code.codeSize() / 1024);
		// Exclude NOPs from size count.
		int size_no_nops = 0;
		for (int i = 0; i < size; i++)
		{
			if (script->zasm[i].command != NOP)
				size_no_nops += 1;
		}
		debug_handle->printf("ZASM instructions:  %zu\n", size_no_nops);
		debug_handle->print("\n");

		if (!uncompiled_command_counts.empty())
		{
			debug_handle->print("=== uncompiled commands:\n");
			for (auto &it : uncompiled_command_counts)
			{
				debug_handle->printf("%s: %d\n", script_debug_command_to_string(it.first).c_str(), it.second);
			}
			debug_handle->print("\n");
		}

		debug_handle->print(
			CConsoleLoggerEx::COLOR_WHITE | CConsoleLoggerEx::COLOR_INTENSITY |
				CConsoleLoggerEx::COLOR_BACKGROUND_BLACK,
			"\nasmjit log / assembly:\n\n");
		debug_handle->print(
			CConsoleLoggerEx::COLOR_BLUE | CConsoleLoggerEx::COLOR_INTENSITY |
				CConsoleLoggerEx::COLOR_BACKGROUND_BLACK,
			logger.data());
	}

	return fn;
}

static void exec_block(JittedScriptHandle *jitted_script)
{
	// pc_t pc = jitted_script->ri->pc;
	pc_t pc = g_pc;
	auto it = jitted_script->fn_handle->codes.find(pc);
	if (it != jitted_script->fn_handle->codes.end())
	{
		it->second();
	}
	else
	{
		auto fn = compile_block(jitted_script, pc);
		jitted_script->fn_handle->codes[pc] = fn;
		fn();
	}
}

int jit_run_script(JittedScriptHandle *jitted_script)
{
	extern int32_t(*stack)[MAX_SCRIPT_REGISTERS];

	if (jitted_script->fn_handle->by_block)
	{
		g_jitted_script = jitted_script;
		g_ret_code = -1;
		g_pc = jitted_script->ri->pc;
		g_registers = jitted_script->ri->d;
		g_stack_base = *stack;
		while (g_ret_code == -1)
		{
			g_sp = jitted_script->ri->sp;
			exec_block(jitted_script);
		}
		jitted_script->ri->sp = g_sp;
		jitted_script->ri->wait_index = g_wait_index;
		jitted_script->ri->pc = g_pc;
		return g_ret_code;
	}

	auto fn = jitted_script->fn_handle->fn;
	return fn(
		jitted_script->ri->d, game->global_d,
		*stack, &jitted_script->ri->sp,
		&jitted_script->ri->pc,
		jitted_script->call_stack_rets, &jitted_script->call_stack_ret_index,
		&jitted_script->ri->wait_index);
}

void jit_delete_script_handle(JittedScriptHandle *jitted_script)
{
	delete jitted_script;
}

void jit_release(JittedFunction fn)
{
	auto fn_handle = (JittedFunctionHandle*)fn;
	rt.release(fn_handle->fn);
	delete fn_handle;
}
