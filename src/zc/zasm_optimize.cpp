#include "zc/zasm_optimize.h"
#include "base/general.h"
#include "base/zdefs.h"
#include "parser/parserDefs.h"
#include "zc/ffscript.h"
#include "zc/script_debug.h"
#include "zc/zasm_utils.h"
#include <cstdint>
#include <optional>
#include <utility>

template<class... Args>
bool one_of(const unsigned int var, const Args&... args)
{
  return ((var == args) || ...);
}

static void expected_to_str(ffscript* s, size_t len)
{
	for (int i = 0; i < len; i++)
	{
		printf("%s\n", script_debug_command_to_string(s[i].command, s[i].arg1, s[i].arg2).c_str());
	}
}

static void expect(std::string name, script_data* script, std::vector<ffscript> s)
{
	bool success = script->size == s.size();
	for (int i = 0; i < s.size(); i++)
	{
		if (!success) break;
		if (script->zasm[i].command == NOP && s[i].command == NOP)
			continue;
		if (script->zasm[i] != s[i])
			success = false;
	}

	if (!success)
	{
		printf("failure: %s\n", name.c_str());
		printf("\n== expected:\n\n");
		expected_to_str(&s[0], s.size());
		printf("\n== got:\n\n");
		expected_to_str(&script->zasm[0], script->size);
	}
}
struct OptContext
{
	uint32_t saved;
	script_data* script;
	ZasmFunction fn;
	ZasmCFG cfg;
	std::vector<pc_t> block_starts;
};

static void remove(OptContext& ctx, pc_t start, pc_t final)
{
	for (int i = start; i <= final; i++)
		ctx.script->zasm[i].command = NOP;
	ctx.saved += final - start + 1;
}

static void optimize_by_block(OptContext& ctx, std::function<void(pc_t, pc_t, pc_t)> cb)
{
	for (pc_t i = 0; i < ctx.block_starts.size(); i++)
	{
		pc_t start_pc = ctx.block_starts[i];
		pc_t final_pc = i == ctx.block_starts.size() - 1 ?
			ctx.fn.final_pc :
			ctx.block_starts[i + 1] - 1;
		cb(i, start_pc, final_pc);
	}
}

static void optimize_pushr(OptContext& ctx)
{
	optimize_by_block(ctx, [&](pc_t block_index, pc_t start_pc, pc_t final_pc){
		for (int j = start_pc; j <= final_pc; j++)
		{
			int command = ctx.script->zasm[j].command;
			int arg1 = ctx.script->zasm[j].arg1;
			if (command != PUSHR) continue;

			int start = j;
			int end = j + 1;
			while (end < final_pc)
			{
				int next_command = ctx.script->zasm[end].command;
				int next_arg1 = ctx.script->zasm[end].arg1;
				if (next_command != PUSHR || next_arg1 != arg1)
					break;

				end++;
			}

			int count = end - start;
			if (count > 1)
			{
				ctx.script->zasm[start].command = PUSHARGSR;
				ctx.script->zasm[start].arg2 = count;
				for (int i = start + 1; i < end; i++)
					ctx.script->zasm[i].command = NOP;
				ctx.saved += count - 1;
				j = end - 1;
			}
		}
	});
}

static void optimize_pop(OptContext& ctx)
{
	optimize_by_block(ctx, [&](pc_t block_index, pc_t start_pc, pc_t final_pc){
		for (int j = start_pc; j <= final_pc; j++)
		{
			int command = ctx.script->zasm[j].command;
			int arg1 = ctx.script->zasm[j].arg1;
			if (command != POP) continue;

			int start = j;
			int end = j + 1;
			while (end < final_pc)
			{
				int next_command = ctx.script->zasm[end].command;
				int next_arg1 = ctx.script->zasm[end].arg1;
				if (next_command != POP || next_arg1 != arg1)
					break;

				end++;
			}

			int count = end - start;
			if (count > 1)
			{
				ctx.script->zasm[start].command = POPARGS;
				ctx.script->zasm[start].arg2 = count;
				for (int i = start + 1; i < end; i++)
					ctx.script->zasm[i].command = NOP;
				ctx.saved += count - 1;
				j = end - 1;
			}
		}
	});
}

// SETV, PUSHR -> PUSHV
// Ex:
//   SETV            D2              5420000
//   PUSHR           D2
// ->
//   PUSHV           D2              5420000
static void optimize_setv_pushr(OptContext& ctx)
{
	optimize_by_block(ctx, [&](pc_t block_index, pc_t start_pc, pc_t final_pc){
		for (int j = start_pc; j < final_pc; j++)
		{
			if (ctx.script->zasm[j].command != SETV) continue;
			if (ctx.script->zasm[j + 1].command != PUSHR) continue;
			if (ctx.script->zasm[j].arg1 != ctx.script->zasm[j + 1].arg1) continue;

			// `zasm_construct_structured` is sensitive to a PUSH being just before
			// a function call, so unlike other places assign the NOP to the first
			// instruction.
			ctx.script->zasm[j + 1] = {PUSHV, ctx.script->zasm[j].arg2};
			ctx.script->zasm[j].command = NOP;
			ctx.saved += 1;
		}
	});
}

// Find pairs of SET, COMPARE
// TODO: use for reducing SETTRUE
// freedom_in_chains.qst/zasm-ffc-1.txt
//   380: COMPARER        D3              D2           
//   381: SETMORE         D2                           
//   382: COMPAREV        D2              0            
//   383: SETTRUE         D2                           
static std::optional<std::pair<int, pc_t>> reduce_comparison(OptContext& ctx, pc_t start_pc, pc_t final_pc, int cmp = 0)
{
	// if (!one_of(ctx.script->zasm[start_pc + 2].command, COMPAREV, COMPAREV2))
	// 	return std::nullopt;

	int j = start_pc;
	int k = j + 1;
	for (; k <= final_pc; k++)
	{
		int command = ctx.script->zasm[k].command;

		ASSERT(command != COMPAREV2); // TODO
		switch (command)
		{
			case COMPAREV:
			{
				ASSERT(ctx.script->zasm[k - 1].command != COMPAREV);
				if (ctx.script->zasm[k].arg2 == 0)
					cmp = INVERT_CMP(cmp);
			}
			break;

			case SETFALSEI:
			case SETLESSI:
			case SETMOREI:
			case SETTRUEI:
			case SETCMP:
			case SETFALSE:
			case SETLESS:
			case SETMORE:
			case SETTRUE:
			{
				if (k + 1 > final_pc || ctx.script->zasm[k + 1].command != COMPAREV)
				{
					// ASSERT(false);
					// return std::nullopt;
				}

				// These should only ever appear once.
				#define ASSIGN(v) {cmp |= v;}
				if (!cmp)
				{
					if (command == SETCMP) ASSIGN(ctx.script->zasm[k].arg2 & CMP_FLAGS)
					if (command == SETLESS || command == SETLESSI) ASSIGN(CMP_LE)
					if (command == SETMORE || command == SETMOREI) ASSIGN(CMP_GE)
					if (command == SETFALSE || command == SETFALSEI) ASSIGN(CMP_NE)
				}
				else
				{
					if (command == SETCMP && (ctx.script->zasm[k].arg2 & CMP_FLAGS) == CMP_NE)
						cmp = INVERT_CMP(cmp);
				}

				// SETTRUE may be first after the COMPARER, in which case it matters.
				// Otherwise, it's a silly nop.
				if ((command == SETTRUE || command == SETTRUEI) && !cmp) cmp = CMP_EQ;
			}
			break;

			case CASTBOOLF:
			case CASTBOOLI:
				if (!cmp) cmp = CMP_EQ;
				break;

			case GOTOTRUE:
			case GOTOMORE:
			case GOTOLESS:
			{
				return std::make_pair(cmp, k);
			}
			break;

			case GOTOFALSE:
			{
				return std::make_pair(INVERT_CMP(cmp), k);
			}
			break;

			default:
			{
				return std::nullopt;
			}
		}

		// if (k == final_pc)
		// 	return std::make_pair(cmp, k);
	}

	// if (k == final_pc)
	return std::make_pair(cmp, k);
}

static pc_t get_block_end(const OptContext& ctx, int block_index)
{
	return block_index == ctx.block_starts.size() - 1 ?
		ctx.fn.final_pc :
		ctx.block_starts.at(block_index + 1) - 1;
}

static std::optional<int> find_next_comp_op(const OptContext& ctx, pc_t start, pc_t end)
{
	for (int i = start; i <= end; i++)
	{
		if (one_of(ctx.script->zasm[i].command, COMPAREV, COMPAREV2, COMPARER, CASTBOOLF))
		{
			return i;
		}
	}

	return std::nullopt;
}

static void optimize_compare(OptContext& ctx)
{
	optimize_by_block(ctx, [&](pc_t block_index, pc_t start_pc, pc_t final_pc){
		for (int j = start_pc; j < final_pc; j++)
		{
			const auto& edges = ctx.cfg.block_edges.at(block_index);
			if (edges.size() != 2)
				continue;

			int command = ctx.script->zasm[j].command;
			if (!one_of(command, COMPARER, COMPAREV, COMPAREV2)) continue;

			auto maybe_cmp = reduce_comparison(ctx, j, final_pc);
			if (!maybe_cmp)
				continue;

			auto [head_block_cmp, k] = maybe_cmp.value();

			// Useful binary search tool for finding problems.
			// static int c = 0;
			// c++;
			// if (!(0 <= c && c < 100000))
			// {
			// 	continue;
			// }
			// if (ctx.script) break;

			// TODO:
			// follow the GOTOTRUE branch recursivley, until find:
			//     D2 not used by COMPARE (so: LOAD D2 ... would invalidate it)
			// Then we have:
			// Head block (block 0)
			// Block 1
			// Block 2
			// ...
			// Block N
			// End block
			// And want to map blocks 0 to N to jump to the End block instead
			

			int start = j;
			int final = k;
			int count = k - j + 1;
			int goto_pc = ctx.script->zasm[final].arg1;

			pc_t block_1 = edges[0];
			pc_t block_2 = edges[1];
			pc_t block_1_start = ctx.block_starts.at(block_1);
			pc_t block_1_final = get_block_end(ctx, block_1);
			pc_t block_2_start = ctx.block_starts.at(block_2);
			pc_t block_2_final = get_block_end(ctx, block_2);
			auto cmd1 = ctx.script->zasm[block_1_start];
			auto cmd2 = ctx.script->zasm[block_2_start];

			if (cmd2.command == COMPAREV)
			{
				if (ctx.cfg.block_edges.at(block_1).size() != 1)
				{
					// TODO
					continue;
				}
				if (ctx.cfg.block_edges.at(block_2).size() != 2)
				{
					// TODO
					continue;
				}

				pc_t post_block_pc = ctx.block_starts.at(ctx.cfg.block_edges.at(block_2).back());

				auto maybe_cmp = reduce_comparison(ctx, find_next_comp_op(ctx, block_2_start, block_2_final).value()-1, block_2_final, head_block_cmp);
				// ASSERT(maybe_cmp);
				if (maybe_cmp)
					head_block_cmp = maybe_cmp->first;
				goto_pc = post_block_pc;

				if (ctx.script->zasm[goto_pc].command == COMPAREV)
				{
					// Even the block after this conditional is reusing the
					// initial condition comparison register (see 54010 below).
					// There's a recursive problem here it seems.
					// Ex: stellar_seas_randomizer.qst/zasm-ffc-123-RandomizerMenu.txt
					/*
						53929: COMPAREV        D2              0            
						53930: GOTOTRUE        53983                        
						53931: PUSHR           D4                           [Block 12 -> 13]
						53932: PUSHV           53975                        
						...
						53978: COMPARER        D3              D2           
						53979: SETMORE         D2                           
						53980: COMPAREV        D2              0            
						53981: SETTRUEI        D2                           
						53982: CASTBOOLF       D2                           
						53983: COMPAREV        D2              1            [Block 13 -> 14, 15]
						53984: SETMOREI        D2                           
						53985: COMPAREV        D2              0            
						53986: GOTOTRUE        54010                        
						53987: SETR            D6              D4           [Block 14 -> 15]
						...
						54007: COMPARER        D2              D3           
						54008: SETTRUEI        D2                           
						54009: CASTBOOLF       D2                           
						54010: COMPAREV        D2              1            [Block 15 -> 16, 17]
						54011: SETMOREI        D2                           
						54012: COMPAREV        D2              0            
					*/	
					continue;
				}

				// static int c = 0;
				// c++;
				// if (!(2025 <= c && c < 2026))
				// {
				// 	continue;
				// }
				// if (ctx.script) continue;

				

				// TODO: is the comparison register always D2?
				int r = D(2);

				// if (cmd1.command == COMPAREV)
				// {
				// 	ASSERT(cmd1.arg1 == r);
				// 	auto maybe_cmp = reduce_comparison(ctx, block_1_start, block_1_final);
				// 	ASSERT(maybe_cmp);

				// 	auto [cmp, final] = maybe_cmp.value();
				// 	int start = block_1_start;
				// 	ctx.script->zasm[start] = ctx.script->zasm[final];
				// 	ctx.script->zasm[start].command = GOTOCMP;
				// 	// ctx.script->zasm[start].arg1 = goto_pc;
				// 	ctx.script->zasm[start].arg2 = cmp;

				// 	for (int i = start + 1; i <= final; i++)
				// 		ctx.script->zasm[i].command = NOP;

				// 	ctx.saved += count;
				// }
				ASSERT(cmd1.command != COMPAREV);
				// ASSERT(ctx.script->zasm[block_1_final].command == SETCMP);
				ASSERT(ctx.script->zasm[block_2_start].command == COMPAREV);
				ASSERT(ctx.script->zasm[block_2_start].arg1 == r);
				// ASSERT(ctx.script->zasm[block_2_start].arg2 == 0);

				// ctx.script->zasm[block_1_final].command = GOTOCMP;
				// ctx.script->zasm[block_1_final].arg1 = post_block_pc;
				// ctx.script->zasm[block_1_final].arg2 = INVERT_CMP(ctx.script->zasm[block_1_final].arg2) & CMP_FLAGS;

				// Find the next compare op
				int first_comp_pc = -1;
				for (int h = block_1_start; h <= block_1_final; h++)
				{
					if (one_of(ctx.script->zasm[h].command, COMPAREV, COMPAREV2, COMPARER, CASTBOOLF))
					{
						first_comp_pc = h;
						break;
					}
				}
				ASSERT(first_comp_pc != -1);
				ASSERT(block_1_final + 1 == block_2_start);

				int cmp;
				if (ctx.script->zasm[first_comp_pc].command == CASTBOOLF)
				{
					auto maybe_cmp = reduce_comparison(ctx, first_comp_pc-1, block_2_final);
					ASSERT(maybe_cmp);
					cmp = maybe_cmp->first;
				}
				else
				{
					auto maybe_cmp = reduce_comparison(ctx, first_comp_pc, block_2_final);
					ASSERT(maybe_cmp);
					cmp = maybe_cmp->first;
				}

				

				// if (cmd2.arg2 == 0)
				// 	cmp = INVERT_CMP(cmp);
				// auto maybe_cmp_1 = reduce_comparison(ctx, first_comp_pc-1, block_1_final);
				// ASSERT(maybe_cmp_1);
				// auto [cmp, k] = maybe_cmp_1.value();
				int m;
				if (ctx.script->zasm[first_comp_pc].command == CASTBOOLF)
				{
					ctx.script->zasm[block_1_final] = {COMPAREV, D(2), 1};
					m = block_1_final + 1;
				}
				else
				{
					m = first_comp_pc + 1;
				}
				ctx.script->zasm[m].command = GOTOCMP;
				ctx.script->zasm[m].arg1 = post_block_pc;
				// ctx.script->zasm[m].arg2 = CMP_EQ;
				ctx.script->zasm[m].arg2 = cmp;
				
				remove(ctx, m + 1, block_2_final);

				

				// if (cmd2.command == COMPAREV)
				// {
				// 	ASSERT(cmd2.arg1 == r);
				// 	auto maybe_cmp = reduce_comparison(ctx, block_2_start, block_2_final);
				// 	ASSERT(maybe_cmp);

				// 	auto [cmp, final] = maybe_cmp.value();
				// 	int start = block_2_start;
				// 	ctx.script->zasm[start] = ctx.script->zasm[final];
				// 	ctx.script->zasm[start].command = GOTOCMP;
				// 	// ctx.script->zasm[start].arg1 = goto_pc;
				// 	ctx.script->zasm[start].arg2 = cmp;

				// 	for (int i = start + 1; i <= final; i++)
				// 		ctx.script->zasm[i].command = NOP;

				// 	ctx.saved += count;
				// }


				// ASSERT(ctx.script->zasm[block_2_final].command == GOTOTRUE || ctx.script->zasm[block_2_final].command == GOTOFALSE);

				// remove(ctx, first_comp_pc + 2, block_2_final);
				// for (int i = block_2_start; i <= block_2_final; i++)
				// 	ctx.script->zasm[i].command = NOP;

				// For now, skip this. Needs much work.
				// if (ctx.script)
				// 	break;

				// Handle the first edge.



				// Affirm that this is a negating COMPAREV.
				// if (cmd2.arg2 != 0)
				// 	continue;

				// // TODO: test case for when cmd2.arg2 == 1
				// // stellar_seas_randomizer.qst/zasm-ffc-1-SolarElemental.txt
				// // 176: COMPARER        D2              D3           
				// // 177: SETTRUEI        D2                           
				// // 178: COMPAREV        D2              0            
				// // 179: GOTOTRUE        186                          
				// // 180: SETR            D6              D4           [Block 6 -> 7]
				// // 181: ADDV            D6              60000        
				// // 182: LOADI           D2              D6           
				// // 183: COMPAREV        D2              0            
				// // 184: SETTRUEI        D2                           
				// // 185: CASTBOOLF       D2                           
				// // 186: COMPAREV        D2              1            [Block 7 -> 8, 9]
				// // 187: SETMOREI        D2                           
				// // 188: COMPAREV        D2              0            
				// // 189: GOTOTRUE        195                     

				// if (ctx.script->zasm[block_2_start + 3].command != GOTOTRUE)
				// 	break;
				// // Test case for above.
				// /*
				// 3342: COMPARER        D3              D2           
				// 3343: SETCMP          D2              12           
				// 3344: COMPAREV        D2              0            
				// 3345: GOTOTRUE        3352                         
				// 3346: LOADD           D2              50000        [Block 105 -> 106]
				// 3347: PUSHR           D2                           
				// 3348: LOADD           D2              20000        
				// 3349: POP             D3                           
				// 3350: COMPARER        D3              D2           
				// 3351: SETCMP          D2              12           
				// 3352: COMPAREV        D2              0            [Block 106 -> 107, 108]
				// 3353: SETCMP          D2              11           
				// 3354: COMPAREV        D2              0            
				// 3355: GOTOFALSE       3368                         
				// */

				// // Fix the first edge.

				// // Find the last instruction of the first block.
				// pc_t block_1_end = block_1 == ctx.block_starts.size() - 1 ?
				// 	ctx.fn.final_pc :
				// 	ctx.block_starts.at(block_1 + 1) - 1;
				// auto& block_1_end_cmd = ctx.script->zasm[block_1_end];

				// ASSERT(block_1_end_cmd.command == SETCMP);
				// block_1_end_cmd.command = GOTOCMP;
				// block_1_end_cmd.arg1 = ctx.block_starts.at(ctx.cfg.block_edges.at(block_2).at(1));
				// block_1_end_cmd.arg2 = INVERT_CMP(block_1_end_cmd.arg2);

				// // Fix the second edge.
				// ASSERT(ctx.script->zasm[block_2_start + 1].command == SETCMP);
				// ASSERT(ctx.script->zasm[block_2_start + 2].command == COMPAREV);
				// ASSERT(ctx.script->zasm[block_2_start + 2].arg2 == 0);
				// ASSERT(ctx.script->zasm[block_2_start + 3].command == GOTOTRUE);

				// ctx.script->zasm[block_2_start] = {GOTOCMP, block_1_end_cmd.arg1, INVERT_CMP(ctx.script->zasm[block_2_start + 1].arg2)};

				// for (int i = 0; i < 4; i++)
				// 	ctx.script->zasm[block_2_start + i].command = NOP;
				// ctx.saved += 4;

				// // Lastly
				// goto_pc = post_block_pc;
			}

			// ctx.script->zasm[start + 1] = ctx.script->zasm[final];
			ctx.script->zasm[start + 1].command = GOTOCMP;
			ctx.script->zasm[start + 1].arg1 = goto_pc;
			ctx.script->zasm[start + 1].arg2 = head_block_cmp;

			for (int i = start + 2; i <= final; i++)
				ctx.script->zasm[i].command = NOP;

			ctx.saved += count;
			j += count;
		}
	});
}

static void optimize_unreachable_blocks(OptContext& ctx)
{
	std::set<pc_t> seen_ids;
	std::set<pc_t> pending_ids = {0};
	while (pending_ids.size())
	{
		pc_t id = *pending_ids.begin();
		pending_ids.erase(pending_ids.begin());
		seen_ids.insert(id);

		for (auto calls_id : ctx.cfg.block_edges.at(id))
		{
			if (!seen_ids.contains(calls_id))
				pending_ids.insert(calls_id);
		}
	}
	for (int i = 0; i < ctx.cfg.block_starts.size(); i++)
	{
		if (!seen_ids.contains((i)))
		{
			int start = ctx.block_starts.at(i);
			int end = i == ctx.cfg.block_starts.size() - 1 ? ctx.fn.final_pc : ctx.block_starts.at(i + 1) - 1;
			for (int j = start; j <= end; j++)
			{
				ctx.script->zasm[j].command = NOP;
			}
			ctx.saved += end - start + 1;

			// Don't ever remove the end marker.
			if (end == ctx.script->size - 1)
			{
				ctx.script->zasm[end].command = 0xFFFF;
				ctx.saved -= 1;
			}
		}
	}
}

static int optimize_function(script_data* script, const ZasmFunction& fn)
{
	OptContext ctx;
	ctx.saved = 0;
	ctx.script = script;
	ctx.fn = fn;
	ctx.cfg = zasm_construct_cfg(script, {{fn.start_pc, fn.final_pc}});
	ctx.block_starts = std::vector<pc_t>(ctx.cfg.block_starts.begin(), ctx.cfg.block_starts.end());

	optimize_unreachable_blocks(ctx);
	optimize_pushr(ctx);
	optimize_pop(ctx);
	optimize_setv_pushr(ctx);

	// Ideas for more opt passes

	// Remove unused writes to a D register

	// Always do this last, as it slightly invalidates the CFG.
	optimize_compare(ctx);

	return ctx.saved;
}

int zasm_optimize(script_data* script)
{
	// if (script->meta.script_name != "Prime") return 0;
	auto start_time = std::chrono::steady_clock::now();
	auto structured_zasm = zasm_construct_structured(script);

	int saved = 0;

	// Optimize unused functions.
	// TODO:
	// This works, but by construction there is never a function that is not called
	// (see comment in zasm_construct_structured). Besides, the zscript compiler
	// probably has always done a good job with pruning unused functions.
	// However, this could have an effect if any opt passes are made that remove dead
	// code, so keep the code nearby until then.
	// std::set<pc_t> seen_ids;
	// std::set<pc_t> pending_ids = {0};
	// while (pending_ids.size())
	// {
	// 	pc_t id = *pending_ids.begin();
	// 	pending_ids.erase(pending_ids.begin());
	// 	seen_ids.insert(id);

	// 	for (auto calls_id : structured_zasm.functions[id].calls_functions)
	// 	{
	// 		if (!seen_ids.contains(calls_id))
	// 			pending_ids.insert(calls_id);
	// 	}
	// }

	for (const auto& fn : structured_zasm.functions)
	{
		// if (!seen_ids.contains(fn.id))
		// {
		// 	for (int i = fn.start_pc; i <= fn.final_pc; i++)
		// 		script->zasm[i].command = NOP;
		// 	saved += fn.final_pc - fn.start_pc + 1;
		// 	continue;
		// }

		saved += optimize_function(script, fn);
	}

	// TODO: remove NOPs and update GOTOs.

	auto end_time = std::chrono::steady_clock::now();
	int ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
	int pct = 100.0 * saved / script->size;
	printf("optimize %zu instr, %zu fns, saved %d instr (%d%%), %dms\n", script->size, structured_zasm.functions.size(), saved, pct, ms);
	return saved;
}

void zasm_optimize()
{
	int saved = 0;
	int size = 0;
	auto start_time = std::chrono::steady_clock::now();

	zasm_for_every_script([&](auto script){
		size += script->size;
		saved += zasm_optimize(script);
	});

	if (size == 0)
		return;

	auto end_time = std::chrono::steady_clock::now();
	int ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
	int pct = 100.0 * saved / size;
	printf("\noptimized all scripts. saved %d instr (%d%%), %dms\n", saved, pct, ms);
}

bool zasm_optimize_test()
{
	script_data script(ScriptType::None, 0);
	std::string name;

	{
		name = "PUSHR -> PUSHARGSR (1)";
		ffscript s[] = {
			{PUSHR, D(3)},
			{PUSHR, D(3)},
			{PUSHR, D(3)},
			{PUSHR, D(3)},
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{PUSHARGSR, D(3), 4},
			{NOP},
			{NOP},
			{NOP},
			{0xFFFF},
		});
	}

	{
		name = "PUSHR -> PUSHARGSR (2)";
		ffscript s[] = {
			{PUSHR, D(3)},
			{PUSHR, D(3)},
			{PUSHR, D(2)},
			{PUSHR, D(3)},
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{PUSHARGSR, D(3), 2},
			{NOP},
			{PUSHR, D(2)},
			{PUSHR, D(3)},
			{0xFFFF},
		});
	}

	{
		name = "POP -> POPARGS";
		ffscript s[] = {
			{POP, D(3)},
			{POP, D(3)},
			{POP, D(3)},
			{POP, D(3)},
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{POPARGS, D(3), 4},
			{NOP},
			{NOP},
			{NOP},
			{0xFFFF},
		});
	}

	{
		name = "if COMPAREV w/ SETCMP CMP_LE";
		ffscript s[] = {
			{COMPAREV, D(2), 1337},
			{SETCMP, D(2), CMP_LE},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 5},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{COMPAREV, D(2), 1337},
			{GOTOCMP, 5, CMP_GT},
			{NOP},
			{NOP},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		});
	}

	{
		name = "if COMPARER w/ SETCMP CMP_LE";
		ffscript s[] = {
			{COMPARER, D(3), D(2)},
			{SETCMP, D(2), CMP_LE},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 5},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{COMPARER, D(3), D(2)},
			{GOTOCMP, 5, CMP_GT},
			{NOP},
			{NOP},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		});
	}

	{
		name = "if COMPARER w/ SETCMP CMP_LT | CMP_SETI";
		ffscript s[] = {
			{COMPARER, D(3), D(2)},
			{SETCMP, D(2), CMP_LT | CMP_SETI},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 5},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{COMPARER, D(3), D(2)},
			{GOTOCMP, 5, CMP_GE}, // SETI is removed - not needed.
			{NOP},
			{NOP},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		});
	}

	{
		name = "if-else-if COMPAREV w/ SETCMP CMP_LE";
		ffscript s[] = {
			{COMPAREV, D(2), 1337},
			{SETCMP, D(2), CMP_LE},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 6},

			{TRACEV, 0},
			{GOTO, 12},

			{SETV, D(2), 9999},               // 6
			{COMPAREV, D(2), 1338},
			{SETCMP, D(2), CMP_EQ},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 12},

			{TRACEV, 1},

			{QUIT},                           // 12
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{COMPAREV, D(2), 1337},
			{GOTOCMP, 6, CMP_GT},
			{NOP},
			{NOP},

			{TRACEV, 0},
			{GOTO, 12},

			{SETV, D(2), 9999},               // 6
			{COMPAREV, D(2), 1338},
			{GOTOCMP, 12, CMP_NE},
			{NOP},
			{NOP},

			{TRACEV, 1},

			{QUIT},                           // 12
			{0xFFFF},
		});
	}

	{
		name = "if COMPARER w/ SETFALSE/COMPAREV";
		ffscript s[] = {
			{COMPARER, D(3), D(2)},
			{SETFALSE, D(2)},
			{COMPAREV, D(2), 0},
			{SETTRUE, D(2)},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 7},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{COMPARER, D(3), D(2)},
			{GOTOCMP, 7, CMP_NE},
			{NOP},
			{NOP},
			{NOP},
			{NOP},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		});
	}

	{
		name = "if COMPARER w/ SETLESS/COMPAREV";
		ffscript s[] = {
			{COMPARER, D(3), D(2)},
			{SETLESS, D(2)},
			{COMPAREV, D(2), 0},
			{SETTRUE, D(2)},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 7},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{COMPARER, D(3), D(2)},
			{GOTOCMP, 7, CMP_LE},
			{NOP},
			{NOP},
			{NOP},
			{NOP},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		});
	}

	{
		name = "if COMPARER w/ SETMORE/COMPAREV (1)";
		ffscript s[] = {
			{COMPARER, D(3), D(2)},
			{SETMORE, D(2)},
			{COMPAREV, D(2), 0},
			{SETTRUE, D(2)},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 7},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{COMPARER, D(3), D(2)},
			{GOTOCMP, 7, CMP_GE},
			{NOP},
			{NOP},
			{NOP},
			{NOP},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		});
	}

	{
		name = "if COMPARER w/ SETMORE/COMPAREV (2)";
		ffscript s[] = {
			{COMPARER, D(3), D(2)},
			{SETMORE, D(2)},
			{COMPAREV, D(2), 0},
			{SETTRUE, D(2)},
			{COMPAREV, D(2), 1},
			{GOTOTRUE, 7},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{COMPARER, D(3), D(2)},
			{GOTOCMP, 7, CMP_LT},
			{NOP},
			{NOP},
			{NOP},
			{NOP},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		});
	}

	// This one is tricky. From bumper.zplay:
	//    if(Distance(CenterX(this), CenterY(this), CenterLinkX(), CenterLinkY())<this->TileWidth*8+2&&Link->Z==0){
	// (but w/o the function call)
	//
	// The ZASM does short circuiting logic. Each segment jumps to a common node upon failure, before it then jumps to
	// the block following the non-true edge of the conditional. Instead, I try to detect this scenario and rewrite the
	// each segment to jump directly to the non-true edge. This allowed for simpler usage of COMPARER/GOTOCMP, rather than
	// complicate sharing of D2 across blocks.
	{
		name = "if-short-circuiting COMPARER across blocks (1)";
		ffscript s[] = {
			{COMPARER, D(3), D(2)},                 // 0: [Block 0 -> 1, 2]
			{SETCMP, D(2), CMP_LT | CMP_SETI},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 7},

			{SETR, D(2), LINKZ},                    // 4: [Block 1 -> 2]
			{COMPAREV, D(2), 0},
			{SETCMP, D(2), CMP_EQ | CMP_SETI},

			{COMPAREV, D(2), 0},                    // 7: [Block 2 -> 3, 4]
			{SETCMP, D(2), CMP_NE | CMP_SETI},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 12},

			{TRACEV, 0},                            // 11: [Block 3 -> 4]

			{QUIT},                                 // 12: [Block 4 ->  ]
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{COMPARER, D(3), D(2)},
			{GOTOCMP, 12, CMP_LT},
			{NOP},
			{NOP},

			{SETR, D(2), LINKZ},
			{COMPAREV, D(2), 0},
			{GOTOCMP, 12, CMP_NE},

			{NOP},
			{NOP},
			{NOP},
			{NOP},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		});
	}

	{
		name = "if-short-circuiting COMPARER across blocks (2)";
		ffscript s[] = {
			{COMPARER, D(2), D(3)},                 // 0: [Block 0 -> 1, 2]
			{SETTRUEI, D(2)},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 10},

			{SETR, D(6), D(4)},                     // 4: [Block 1 -> 2]
			{ADDV, D(6), 60000},
			{LOADI, D(2), D(6)},
			{COMPAREV, D(2), 0},
			{SETTRUEI, D(2)},
			{CASTBOOLF, D(2)},

			{COMPAREV, D(2), 1},                    // 10: [Block 2 -> 3, 4]
			{SETMOREI, D(2)},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 15},

			{TRACEV, 0},                            // 14: [Block 3 -> 4]

			{QUIT},                                 // 15: [Block 4 ->  ]
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{COMPARER, D(2), D(3)},
			{GOTOCMP, 15, CMP_EQ},
			{NOP},
			{NOP},

			{SETR, D(6), D(4)},
			{ADDV, D(6), 60000},
			{LOADI, D(2), D(6)},
			{COMPAREV, D(2), 0},
			{GOTOCMP, 15, CMP_NE},
			{NOP},

			{NOP},
			{NOP},
			{NOP},
			{NOP},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		});
	}

	{
		name = "if-short-circuiting COMPARER across blocks (3)";
		ffscript s[] = {
			{COMPARER, D(3), D(2)},                 // 0: [Block 0 -> 1, 2]
			{SETMORE, D(2)},
			{COMPAREV, D(2), 0},
			{SETTRUEI, D(2)},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 11},

			{PUSHR, D(4)},                          // 6: [Block 1 -> 2]
			{PUSHV, 1337},
			{TRACEV, 1337},
			{POP, D(4)},
			{CASTBOOLF, D(2)},

			{COMPAREV, D(2), 1},                    // 11: [Block 2 -> 3, 4]
			{SETMOREI, D(2)},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 16},

			{TRACEV, 0},                            // 15: [Block 3 -> 4]

			{QUIT},                                 // 16: [Block 4 ->  ]
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{COMPARER, D(3), D(2)},
			{GOTOCMP, 16, CMP_LT},
			{NOP},
			{NOP},
			{NOP},
			{NOP},

			{PUSHR, D(4)},
			{PUSHV, 1337},
			{TRACEV, 1337},
			{POP, D(4)},
			{COMPAREV, D(2), 1},

			{GOTOCMP, 16, CMP_NE},
			{NOP},
			{NOP},
			{NOP},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		});
	}

	{
		name = "if-short-circuiting COMPARER across blocks (4)";
		ffscript s[] = {
			{COMPARER, D(3), D(2)},                 // 0: [Block 0 -> 1, 2]
			{SETMOREI, D(2)},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 15},

			{SETR, D(6), D(4)},                     // 4: [Block 1 -> 2]
			{ADDV, D(6), 30000},
			{LOADI, D(2), D(6)},
			{PUSHR, D(2)},
			{SETV, D(2), 386050000},
			{POP, D(3)},
			{COMPARER, D(3), D(2)},
			{SETMORE, D(2)},
			{COMPAREV, D(2), 0},
			{SETTRUEI, D(2)},
			{CASTBOOLF, D(2)},

			{COMPAREV, D(2), 1},                    // 15: [Block 2 -> 3, 4]
			{SETMOREI, D(2)},
			{COMPAREV, D(2), 0},
			{GOTOTRUE, 20},

			{TRACEV, 0},                            // 19: [Block 3 -> 4]

			{QUIT},                                 // 20: [Block 4 ->  ]
			{0xFFFF},
		};
		script.zasm = s;
		script.recalc_size();
		zasm_optimize(&script);

		expect(name, &script, {
			{COMPARER, D(3), D(2)},
			{GOTOCMP, 20, CMP_GE},
			{NOP},
			{NOP},

			{SETR, D(6), D(4)},
			{ADDV, D(6), 30000},
			{LOADI, D(2), D(6)},
			{PUSHR, D(2)},
			{SETV, D(2), 386050000},
			{POP, D(3)},
			{COMPARER, D(3), D(2)},
			{GOTOCMP, 20, CMP_GE},
			{NOP},
			{NOP},
			{NOP},

			{NOP},
			{NOP},
			{NOP},
			{NOP},

			{TRACEV, 0},

			{QUIT},
			{0xFFFF},
		});
	}

	// TODO ! tests
	/*

		8088: COMPARER        D3              D2           
		8089: SETMORE         D2                           
		8090: COMPAREV        D2              0            
		8091: SETTRUEI        D2                           
		8092: COMPAREV        D2              0            
		8093: GOTOTRUE        8105                         
		8094: SETR            D2              LINKY        [Block 6 -> 7]
		8095: PUSHR           D2                           
		8096: SETR            D6              D4           
		8097: ADDV            D6              20000        
		8098: LOADI           D2              D6           
		8099: POP             D3                           
		8100: COMPARER        D3              D2           
		8101: SETMORE         D2                           
		8102: COMPAREV        D2              0            
		8103: SETTRUEI        D2                           
		8104: CASTBOOLF       D2                           
		8105: COMPAREV        D2              1            [Block 7 -> 22]
		8106: SETMOREI        D2                           
		8107: GOTO            8224                         
		8108: GOTO            8223                         [Block 8 -> 21]

		-------



		779: COMPARER        D2              D3           
		780: SETTRUEI        D2                           
		781: COMPAREV        D2              0            
		782: GOTOTRUE        802                          
		783: SETR            D6              D4           [Block 37 -> 38, 39]
		784: ADDV            D6              90000        
		785: LOADI           D2              D6           
		786: COMPAREV        D2              1            
		787: GOTOMORE        800                          
		788: SETR            D6              D4           [Block 38 -> 39]
		789: ADDV            D6              50000        
		790: LOADI           D2              D6           
		791: PUSHR           D2                           
		792: SETV            D2              320000       
		793: POP             D3                           
		794: COMPARER        D3              D2           
		795: SETLESS         D2                           



		-------


		54601: COMPARER        D2              D3           
		54602: SETTRUEI        D2                           
		54603: COMPAREV        D2              0            
		54604: GOTOTRUE        54617                        
		54605: SETV            D2              130000       [Block 1559 -> 1560]
		54606: PUSHR           D2                           
		54607: POP             D0                           
		54608: SETR            D2              SCREENSTATED 
		54609: PUSHR           D2                           
		54610: SETV            D2              10000        
		54611: POP             D3                           
		54612: CASTBOOLF       D2                           
		54613: CASTBOOLF       D3                           
		54614: COMPARER        D2              D3           
		54615: SETTRUEI        D2                           
		54616: CASTBOOLF       D2                           
		54617: COMPAREV        D2              1            [Block 1560 -> 1561]
		54618: SETMOREI        D2                           
		54619: CASTBOOLF       D2                           
		54620: COMPAREV        D2              1       


		------

		435: COMPARER        D3              D2           
		436: SETMORE         D2                           
		437: COMPAREV        D2              0            
		438: SETTRUEI        D2                           
		439: CASTBOOLF       D2                           
		440: COMPAREV        D2              1            [Block 12 -> 13, 14]
		441: SETMOREI        D2                           
		442: SETR            D6              D4           
		443: ADDV            D6              90000        
		444: STOREI          D2              D6           
		445: SETR            D6              D4           
		446: ADDV            D6              190000       
		447: LOADI           D2              D6           
		448: COMPAREV        D2              0            
		449: GOTOTRUE        458                          

	*/

	script.zasm = nullptr;
	return true;
}
