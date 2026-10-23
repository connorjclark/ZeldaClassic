#include "zc/scripting/jit/jit.h"
#include "components/zasm/pc.h"

void jit_startup_impl()
{
}

JittedScript* jit_compile_script([[maybe_unused]] zasm_script* script)
{
	return nullptr;
}

bool jit_precompile_scripts_impl([[maybe_unused]] const std::vector<zasm_script*>& scripts, [[maybe_unused]] const std::function<void(zasm_script*, JittedScript*)>& on_compiled)
{
	return false;
}

bool jit_start_background_compile_impl([[maybe_unused]] const std::vector<zasm_script*>& scripts, [[maybe_unused]] const std::function<void(zasm_script*, JittedScript*)>& on_compiled)
{
	return false;
}

void jit_poll_background_compile_impl()
{
}

void jit_cancel_background_compile_impl()
{
}

int jit_run_script([[maybe_unused]] JittedScriptInstance* j_instance)
{
	return RUNSCRIPT_ERROR;
}

JittedScriptInstance* jit_create_script_impl([[maybe_unused]] script_data *script, [[maybe_unused]] refInfo* ri, [[maybe_unused]] JittedScript* j_script)
{
	return nullptr;
}

void jit_profiler_increment_function_back_edge([[maybe_unused]] JittedScriptInstance* j_instance, [[maybe_unused]] pc_t pc)
{
}

void jit_release([[maybe_unused]] JittedScript* j_script)
{
}
