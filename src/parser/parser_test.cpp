#include "fmt/base.h"
#include "fmt/ranges.h"
#include "parser/Compiler.h"
#include "test_runner/assert.h"
#include "test_runner/test_runner.h"
#include "parser/config.h"
#include "base/zsys.h"
#include "zasm/debug_data.h"
#include <functional>
#include <memory>
#include <set>

using namespace std::literals::string_literals;

void updateIncludePaths();

static std::unique_ptr<ZScript::ScriptsData> runCompiler(std::string script_path)
{
	bool has_qrs = false;
	ZScript::ScriptParser::initialize(has_qrs);
	updateIncludePaths();
	ZScript::CompileOption::OPT_NO_ERROR_HALT.setDefault(ZScript::OPTION_ON);

	bool metadata = true;
	bool docs = false;
	return ZScript::compile(script_path, metadata, docs);
}

static void TEST(std::string name, TestResults& tr, std::function<bool()> cb)
{
	try
	{
		if (!cb())
			tr.failed++;
	}
	catch (std::string err)
	{
		fmt::println("[{}] error: {}", name, err);
		tr.failed++;
	}

	tr.total++;
}

static const DebugScope* resolveFileScope(const DebugData& debugData, std::string fname)
{
	const DebugScope* scope = debugData.resolveFileScope(fname);
	if (!scope)
		throw fmt::format("could not find file scope: {}", fname);

	return scope;
}

static const DebugScope* resolveScope(const DebugData& debugData, std::string identifier, const DebugScope* current_scope)
{
	const DebugScope* scope = debugData.resolveScope(identifier, current_scope);
	if (!scope)
		throw fmt::format("could not find scope: {} (scope: {})", identifier, debugData.getFullScopeName(current_scope));

	return scope;
}

static const DebugSymbol* resolveSymbol(const DebugData& debugData, std::string identifier, const DebugScope* current_scope)
{
	const DebugSymbol* symbol = debugData.resolveSymbol(identifier, current_scope);
	if (!symbol)
		throw fmt::format("could not find symbol: {} (scope: {})", identifier, debugData.getFullScopeName(current_scope));

	return symbol;
}

static const DebugType* resolveSymbolTypeUnwrap(const DebugData& debugData, std::string identifier, const DebugScope* current_scope)
{
	const DebugSymbol* symbol = debugData.resolveSymbol(identifier, current_scope);
	if (!symbol)
		throw fmt::format("could not find symbol: {} (scope: {})", identifier, debugData.getFullScopeName(current_scope));

	auto* type = debugData.getType(symbol->type_id);
	while (type->tag == TYPE_CONST || type->tag == TYPE_ARRAY)
		type = debugData.getType(type->extra);
	return type;
}

static std::string resolveSymbolTypeName(const DebugData& debugData, std::string identifier, const DebugScope* current_scope)
{
	const DebugSymbol* symbol = debugData.resolveSymbol(identifier, current_scope);
	if (!symbol)
		throw fmt::format("could not find symbol: {} (scope: {})", identifier, debugData.getFullScopeName(current_scope));

	return debugData.getTypeName(symbol->type_id);
}

static std::vector<const DebugSymbol*> getAllSymbolsWithin(const DebugData& debugData, const DebugScope* current_scope)
{
	std::vector<const DebugSymbol*> symbols;
	std::vector<const DebugScope*> scopes = {current_scope};
	std::set<const DebugScope*> seen;

	while (!scopes.empty())
	{
		auto* scope = scopes.back();
		scopes.pop_back();
		seen.insert(scope);

		for (auto* symbol : debugData.getChildSymbols(scope))
			symbols.push_back(symbol);

		auto children = debugData.getChildScopes(scope);
		for (auto* child : children)
			if (!seen.contains(child)) scopes.push_back(child);
	}

	return symbols;
}

static std::string symbolsToString(const DebugData& debugData, const std::vector<const DebugSymbol*>& symbols)
{
	std::vector<std::string> parts;

	for (auto* symbol : symbols)
		parts.push_back(debugData.getDebugSymbolName(symbol));

	return fmt::format("{}", fmt::join(parts, ", "));
}

TestResults test_parser(bool verbose)
{
	TestResults tr{};

	int test_zc_arg = zapp_check_switch("-test-zc", {"test_dir"});
	CHECK(test_zc_arg > 0);
	std::string test_dir = zapp_get_arg_string(test_zc_arg + 1);

	TEST("debug data scopes", tr, [&]{
		auto results = runCompiler(test_dir + "/scripts/playground/auto/scopes.zs");
		if (!results->success)
			return false;

		// Roundtrip.
		auto encoded_debug_data = results->zasmCompilerResult.debugData.encode();
		results->zasmCompilerResult.debugData = DebugData::decode(encoded_debug_data).value();

		auto& debugData = results->zasmCompilerResult.debugData;
		if (debugData.source_files.empty())
			return false;
		if (debugData.scopes.empty())
			return false;

		printf("%s\n", debugData.internalToStringForDebugging().c_str());

		const DebugScope* root_scope = &debugData.scopes[0];
		if (root_scope->tag != TAG_ROOT)
			return false;

		std::string scopes_fname;
		for (const auto& fname : debugData.source_files)
		{
			if (fname.path.ends_with("tests/scripts/playground/auto/scopes.zs"))
			{
				scopes_fname = fname.path;
				break;
			}
		}

		const DebugScope* file_scope = resolveFileScope(debugData, scopes_fname);
		const DebugScope* a_namespace_scope = resolveScope(debugData, "A", file_scope);
		const DebugScope* a_b_namespace_scope = resolveScope(debugData, "A::B", file_scope);
		const DebugScope* d_namespace_scope = resolveScope(debugData, "D", file_scope);
		const DebugScope* a_fn_scope = resolveScope(debugData, "A::A_fn", file_scope);
		const DebugScope* a_b_fn_scope = resolveScope(debugData, "A::B::B_fn", file_scope);
		const DebugScope* a_cl_class_scope = resolveScope(debugData, "A::CL", file_scope);
		const DebugScope* a_trace_fn_scope = resolveScope(debugData, "A::trace", file_scope);
		const DebugScope* a_cl_ctor_fn_scope = resolveScope(debugData, "A::CL::CL", file_scope);
		const DebugScope* a_cl_trace_fn_scope = resolveScope(debugData, "A::CL::trace", file_scope);

		std::set<const DebugScope*> scopes = {file_scope, a_namespace_scope, a_b_namespace_scope, d_namespace_scope, a_fn_scope, a_b_fn_scope, a_cl_class_scope, a_trace_fn_scope, a_cl_ctor_fn_scope, a_cl_trace_fn_scope};
		if (scopes.size() != 10)
			return false;

		if (debugData.resolveScope("unused_fn", file_scope))
			throw "expected unused functions to not be in debug data"s;

		const DebugSymbol* global_var_never_shadowed = resolveSymbol(debugData, "GLOBAL_VAR_NEVER_SHADOWED", root_scope);
		if (&debugData.scopes[global_var_never_shadowed->scope_index] != file_scope)
			return false;

		// Global scopes and symbols are reachable everywhere.
		for (auto* scope : scopes)
		{
			resolveScope(debugData, "Trace", scope); // internal
			resolveScope(debugData, "Above", scope); // std
			resolveSymbol(debugData, "globalThing", scope); // tests/scripts/playground/auto/global_objects.zs

			const DebugSymbol* symbol = resolveSymbol(debugData, "GLOBAL_VAR_NEVER_SHADOWED", scope);
			if (symbol != global_var_never_shadowed)
				return false;
		}

		const DebugSymbol* global_var = resolveSymbol(debugData, "GLOBAL_VAR", root_scope);
		if (&debugData.scopes[global_var->scope_index] != file_scope)
			return false;

		if (resolveSymbol(debugData, "GLOBAL_VAR", file_scope) != global_var)
			return false;

		// Check variable shadowing.
		for (auto* scope : {a_namespace_scope, a_b_namespace_scope, a_fn_scope, a_b_fn_scope})
		{
			const DebugSymbol* symbol = resolveSymbol(debugData, "GLOBAL_VAR", scope);
			if (symbol == global_var)
				return false;
		}

		assertEqual(resolveSymbol(debugData, "GLOBAL_VAR", a_fn_scope)->storage, LOC_STACK);

		auto a_b_fn_child_scopes = debugData.getChildScopes(a_b_fn_scope);
		assertSize(a_b_fn_child_scopes, 1);

		if (resolveSymbol(debugData, "GLOBAL_VAR", a_b_fn_child_scopes[0])->storage != LOC_STACK)
			return false;

		// using namespace.
		const DebugScope* using_namespace_statement_fn_scope = resolveScope(debugData, "using_namespace_statement", file_scope);
		const DebugScope* using_namespace_statement_fn_child_scope = debugData.getChildScopes(using_namespace_statement_fn_scope)[0];
		resolveSymbol(debugData, "A_var", using_namespace_statement_fn_child_scope);
		resolveSymbol(debugData, "D_var", using_namespace_statement_fn_child_scope);
		resolveSymbol(debugData, "D_var", a_cl_class_scope);

		resolveScope(debugData, "loop_fn", file_scope);

		// Symbol and scope with same name can be resolved.
		// Audio (const Audio Audio) and Audio::AdjustMusicVolume (member function of Audio class)
		resolveSymbol(debugData, "Audio", file_scope);
		resolveScope(debugData, "Audio::AdjustMusicVolume", file_scope);

		auto loop_fn_symbols = getAllSymbolsWithin(debugData, resolveScope(debugData, "loop_fn", file_scope));
		assertEqual(symbolsToString(debugData, loop_fn_symbols), "int z @ Stack[0], int y @ Stack[0], int x @ Stack[0], int i @ Stack[0]"s);

		auto do_fn_symbols = getAllSymbolsWithin(debugData, resolveScope(debugData, "do_fn", file_scope));
		assertEqual(symbolsToString(debugData, do_fn_symbols), "int i @ Stack[1], int j @ Stack[0]"s);

		auto range_loop_fn_symbols = getAllSymbolsWithin(debugData, resolveScope(debugData, "range_loop_fn", file_scope));
		assertEqual(symbolsToString(debugData, range_loop_fn_symbols), "int[] arr @ Stack[4], int z @ Stack[3], int __LOOP_ITER @ Stack[3] <HIDDEN>, int[] __LOOP_ARR @ Stack[2] <HIDDEN>, int x @ Stack[1], int y @ Stack[0]"s);

		auto template_fns = debugData.resolveFunctions("template_fn", file_scope);
		assertSize(template_fns, 2);
		assertEqual(symbolsToString(debugData, getAllSymbolsWithin(debugData, template_fns[0])), "char32[] val1 @ Stack[2], int val2 @ Stack[1], char32[][][] double_arr @ Stack[0]"s);
		assertEqual(symbolsToString(debugData, getAllSymbolsWithin(debugData, template_fns[1])), "int val1 @ Stack[2], int val2 @ Stack[1], int[][] double_arr @ Stack[0]"s);

		// types.
		assertEqual(resolveSymbolTypeName(debugData, "CONST_GLOBAL_VAR", file_scope), "const int"s);
		assertEqual(resolveSymbolTypeName(debugData, "GLOBAL_BOOL", file_scope), "bool"s);
		assertEqual(resolveSymbolTypeName(debugData, "A::CL::this_ptr", file_scope), "CL"s);
		assertEqual(resolveSymbolTypeName(debugData, "BITDX_NORMAL", file_scope), "const BlitModeBitflags"s);
		assertEqual(resolveSymbolTypeUnwrap(debugData, "BITDX_NORMAL", file_scope)->tag, TYPE_BITFLAGS);

		// internal global variables.
		resolveSymbol(debugData, "NUM_COMBO_POS", file_scope);

		// aliased variables/functions are marked as hidden.
		assertTrue(!(resolveSymbol(debugData, "eweapon::Power", file_scope)->flags & SYM_FLAG_HIDDEN));
		assertTrue(resolveSymbol(debugData, "eweapon::Damage", file_scope)->flags & SYM_FLAG_HIDDEN);
		assertTrue(resolveSymbol(debugData, "itemdata::Defence", file_scope)->flags & SYM_FLAG_HIDDEN);
		assertTrue(resolveScope(debugData, "TraceFFC", file_scope)->flags & (SCOPE_FLAG_INTERNAL | SCOPE_FLAG_DEPRECATED));
		assertTrue(resolveScope(debugData, "EngineDegtoRad", file_scope)->flags & (SCOPE_FLAG_INTERNAL | SCOPE_FLAG_DEPRECATED));

		// alias function signatures.
		assertEqual(debugData.getFunctionSignature(resolveScope(debugData, "DegToRad", file_scope)), "int DegToRad(int degrees)"s);
		assertEqual(debugData.getFunctionSignature(resolveScope(debugData, "DegtoRad", file_scope)), "int DegtoRad(int degrees)"s);
		assertEqual(debugData.getFunctionSignature(resolveScope(debugData, "EngineDegtoRad", file_scope)), "int EngineDegtoRad(int degrees)"s);

		// enums.
		const DebugScope* enum_scope = resolveScope(debugData, "Direction", file_scope);
		auto enum_symbols = debugData.getChildSymbols(enum_scope);
		assertSize(enum_symbols, 12);
		for (auto* scope : scopes)
		{
			// Named enum.
			resolveSymbol(debugData, "DIR_UP", scope);
			// Anonymous enum.
			resolveSymbol(debugData, "MIDI_NONE", scope);
			// ZScript doesn't actually support scoped access of enum members, but whatever.
			resolveSymbol(debugData, "Direction::DIR_UP", scope);
		}

		// base classes.
		assertEqual(resolveScope(debugData, "sprite", file_scope)->tag, TAG_CLASS);
		assertEqual(resolveScope(debugData, "sprite::OwnObject", file_scope)->tag, TAG_FUNCTION);
		assertEqual(resolveSymbol(debugData, "sprite::X", file_scope)->storage, LOC_REGISTER);
		assertEqual(resolveSymbol(debugData, "sprite::Misc", file_scope)->storage, LOC_REGISTER);
		assertEqual(resolveScope(debugData, "ffc", file_scope)->tag, TAG_CLASS);
		assertEqual(resolveScope(debugData, "ffc::OwnObject", file_scope)->tag, TAG_FUNCTION);
		assertEqual(resolveSymbol(debugData, "ffc::X", file_scope)->storage, LOC_REGISTER);
		assertEqual(resolveSymbol(debugData, "ffc::Misc", file_scope)->storage, LOC_REGISTER);

		// Function signatures.
		assertEqual(debugData.getFunctionSignature(a_b_fn_scope), "void A::B::B_fn()"s);
		assertEqual(debugData.getFunctionSignature(a_cl_trace_fn_scope), "void A::CL::trace()"s);
		assertEqual(debugData.getFunctionSignature(a_cl_ctor_fn_scope), "CL A::CL::CL()"s);
		assertEqual(debugData.getFunctionSignature(resolveScope(debugData, "has_constant", file_scope)), "void has_constant()"s);
		assertEqual(debugData.getFunctionSignature(resolveScope(debugData, "Trace", file_scope)), "void Trace(untyped val)"s);
		assertEqual(debugData.getFunctionSignature(resolveScope(debugData, "printf", file_scope)), "void printf(char32[] format, ...untyped[] values)"s);
		assertEqual(debugData.getFunctionSignature(resolveScope(debugData, "long_fn", file_scope)), "void long_fn(long a)"s);
		assertEqual(debugData.getFunctionSignature(resolveScope(debugData, "trace_all", file_scope)), "void trace_all(int a, ...int[] b)"s);
		assertEqual(debugData.getFunctionSignature(resolveScope(debugData, "trace_all_2", file_scope)), "void trace_all_2(...int[] a)"s);
		assertEqual(debugData.getFunctionSignature(resolveScope(debugData, "enum_fn", file_scope)), "Enum enum_fn(const Enum e)"s);
		assertEqual(debugData.getFunctionSignature(resolveScope(debugData, "ArrayCopy", file_scope)), "void ArrayCopy(T[] dest, T[] src)"s);
		assertEqual(debugData.getFunctionSignature(resolveScope(debugData, "Max", file_scope)), "T Max(...T[] values)"s);
		assertEqual(debugData.getFunctionSignature(template_fns[0]), "void template_fn(char32[] val1, int val2)"s);
		assertEqual(debugData.getFunctionSignature(template_fns[1]), "void template_fn(int val1, int val2)"s);
		// Note: parameters with default values are currently are not supported. Ideally they should
		// produce a new TAG_DEFAULT_PARAM scope at the callsite.
		assertEqual(debugData.getFunctionSignature(a_fn_scope), "void A::A_fn(int GLOBAL_VAR)"s);

		auto num_fns = debugData.resolveFunctions("num_fn", file_scope);
		assertSize(num_fns, 2);
		assertEqual(debugData.getFunctionSignature(num_fns[0]), "void num_fn(int a)"s);
		assertEqual(debugData.getFunctionSignature(num_fns[1]), "void num_fn(long a)"s);

		// Inline/internal functions.
		const DebugScope* max_fn_scope = resolveScope(debugData, "Max", file_scope);
		assertGreaterThan(max_fn_scope->start_pc, (pc_t)0);
		assertGreaterThan(max_fn_scope->end_pc, (pc_t)0);

		const DebugScope* websocket_ctor_scope = resolveScope(debugData, "websocket::websocket", file_scope);
		assertGreaterThan(websocket_ctor_scope->start_pc, (pc_t)0);
		assertGreaterThan(websocket_ctor_scope->end_pc, (pc_t)0);

		const DebugScope* audio_adjust_fn_scope = resolveScope(debugData, "Audio::AdjustMusicVolume", file_scope);
		assertGreaterThan(audio_adjust_fn_scope->start_pc, (pc_t)0);
		assertGreaterThan(audio_adjust_fn_scope->end_pc, (pc_t)0);

		return true;
	});

	return tr;
}
