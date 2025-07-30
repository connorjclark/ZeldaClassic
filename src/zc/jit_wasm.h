#ifndef ZC_JIT_WASM_H_
#define ZC_JIT_WASM_H_

#include "base/zdefs.h"
#include "zc/zasm_utils.h"
#include <cstdint>

struct JittedFunctionHandle
{
	int module_id;
	std::map<pc_t, uint32_t> call_pc_to_return_block_id;
	std::map<pc_t, uint32_t> wait_frame_pc_to_block_id;
};

#define JIT_MAX_CALL_STACK_SIZE 100

struct JittedScriptHandle
{
	JittedFunctionHandle* fn;
	script_data* script;
	refInfo* ri;
	uint32_t handle_id;
	pc_t call_stack_rets[JIT_MAX_CALL_STACK_SIZE];
	pc_t call_stack_ret_index;
	// nth WaitX instruction last execution stopped at. If 0, then the script has not run yet.
	uint32_t wait_index;

	~JittedScriptHandle();
};

#endif
