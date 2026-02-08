// TODO: use DebugData type_id for script_object_type. Complex change w/ many compat
// considerations. I started on branch: type-store

#include "zasm/debug_data.h"
#include "base/ints.h"
#include "zasm/pc.h"
#include "zasm/table.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fmt/ranges.h>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

// Write a signed integer using Variable Length Quantity (ZigZag).
void write_signed_vlq(std::vector<byte>& buf, int32_t value)
{
	uint32_t u_val = (value << 1) ^ (value >> 31);
	do {
		byte byte = u_val & 0x7F;
		u_val >>= 7;
		if (u_val != 0) byte |= 0x80;
		buf.push_back(byte);
	} while (u_val != 0);
}

// Write unsigned VLQ.
void write_unsigned_vlq(std::vector<byte>& buf, uint32_t value)
{
	do {
		byte byte = value & 0x7F;
		value >>= 7;
		if (value != 0) byte |= 0x80;
		buf.push_back(byte);
	} while (value != 0);
}

uint32_t read_uvlq_from_buffer(const std::vector<byte>& buffer, size_t& cursor)
{
	uint32_t result = 0;
	int shift = 0;
	while (cursor < buffer.size())
	{
		byte b = buffer[cursor++];
		result |= (b & 0x7F) << shift;
		if (!(b & 0x80)) return result;
		shift += 7;
	}

	return result;
}

int32_t read_svlq_from_buffer(const std::vector<byte>& buf, size_t& cursor)
{
	uint32_t u_val = read_uvlq_from_buffer(buf, cursor);
	return (u_val >> 1) ^ -(int32_t)(u_val & 1);
}

std::vector<std::string_view> split_identifier(std::string_view str)
{
	std::vector<std::string_view> tokens;
	tokens.reserve(5);

	size_t start = 0;
	size_t end = str.find("::");

	while (end != std::string_view::npos)
	{
		tokens.push_back(str.substr(start, end - start));
		start = end + 2;
		end = str.find("::", start);
	}

	tokens.push_back(str.substr(start));
	return tokens;
}

} // end namespace

void DebugData::appendLineInfoSetFile(int file)
{
	debug_lines_encoded.push_back(DEBUG_LINE_OP_SET_FILE);
	write_unsigned_vlq(debug_lines_encoded, file);
}

void DebugData::appendLineInfoSimpleStep(byte d_pc)
{
	debug_lines_encoded.push_back(d_pc);
}

void DebugData::appendLineInfoExtendedStep(int d_pc, int d_line)
{
	debug_lines_encoded.push_back(DEBUG_LINE_OP_EXTENDED_STEP);
	write_unsigned_vlq(debug_lines_encoded, d_pc);
	write_signed_vlq(debug_lines_encoded, d_line);
}

void DebugData::appendLineInfoPrologueEnd()
{
	debug_lines_encoded.push_back(DEBUG_LINE_OP_PROLOGUE_END);
}

bool DebugData::exists() const
{
	return !source_files.empty();
}

std::pair<const char*, int> DebugData::resolveLocation(pc_t pc) const
{
	if (debug_lines_encoded.empty()) return {};

	size_t cache_index = pc & 1023; 
	if (resolve_location_cache[cache_index].pc == pc)
		return resolve_location_cache[cache_index].result;

	if (!checkpoints_built)
		buildCheckpoints();

	size_t cursor = 0;
	int32_t current_line = 1;
	int32_t current_file = 0;
	pc_t current_pc = 0;
	bool next_is_prologue_end = false;

	// Find the checkpoint with the highest pc <= target pc.
	// upper_bound returns the first element > target.
	auto it = std::upper_bound(checkpoints.begin(), checkpoints.end(), pc,
		[](size_t val, const Checkpoint& cp) { return val < cp.pc; });

	if (it != checkpoints.begin())
	{
		const Checkpoint& cp = *(--it); // Move back to the one <= pc
		current_pc = cp.pc;
		cursor = cp.cursor;
		current_line = cp.line;
		current_file = cp.file_index;
	}

	// Default return value (if loop doesn't match or buffer ends).
	std::pair<const char*, int> result = {
		(source_files.empty() ? "?" : source_files[current_file].path.data()),
		current_line
	};

	while (cursor < debug_lines_encoded.size())
	{
		byte cmd = debug_lines_encoded[cursor++];

		if (cmd == DEBUG_LINE_OP_SET_FILE)
		{
			current_file = read_uvlq_from_buffer(debug_lines_encoded, cursor);
			continue;
		}

		size_t d_pc;
		int32_t d_line;

		if (cmd == DEBUG_LINE_OP_EXTENDED_STEP)
		{
			d_pc = read_uvlq_from_buffer(debug_lines_encoded, cursor);
			d_line = read_svlq_from_buffer(debug_lines_encoded, cursor);
		}
		else
		{
			d_pc = cmd;
			d_line = 1;
		}

		// Does the target pc fall within the range defined by this instruction?
		// Range: [current_pc, current_pc + d_pc)
		if (current_pc + d_pc > pc)
		{
			if (current_file < source_files.size())
				result = {source_files[current_file].path.data(), current_line};
			else
				result = {"?", current_line};
			break;
		}

		current_pc += d_pc;
		current_line += d_line;
		
		// Update default result in case we exit loop (e.g. end of stream)
		if (current_file < source_files.size())
			result = {source_files[current_file].path.data(), current_line};
	}

	resolve_location_cache[cache_index].pc = pc;
	resolve_location_cache[cache_index].result = result;

	return result;
}

void DebugData::buildScopesSorted() const
{
	if (scopes_sorted) return;

	sorted_scopes.clear();
	sorted_scopes.reserve(scopes.size());

	for (size_t i = 0; i < scopes.size(); ++i)
	{
		const auto& s = scopes[i];
		// Only index scopes that actually have code ranges (Function, Block, etc.)
		if (s.end_pc > s.start_pc) 
			sorted_scopes.push_back(i);
	}

	// Sort Key 1: Start PC (Ascending)
	// Sort Key 2: End PC (Descending) -> Ensures Outer scopes appear BEFORE Inner scopes
	//             when they share the same start address.
	std::sort(sorted_scopes.begin(), sorted_scopes.end(), 
		[&](int32_t a, int32_t b) {
			const auto& sa = scopes[a];
			const auto& sb = scopes[b];
			if (sa.start_pc != sb.start_pc)
				return sa.start_pc < sb.start_pc;
			return sa.end_pc > sb.end_pc; 
	});

	scopes_sorted = true;
}

const DebugScope* DebugData::resolveScope(pc_t pc) const
{
	if (!scopes_sorted) buildScopesSorted();

	// Find the first scope that starts strictly after the pc.
	// The scope we want must be before this point.
	auto it = std::upper_bound(sorted_scopes.begin(), sorted_scopes.end(), pc, 
		[&](pc_t val, int32_t idx) {
			return val < scopes[idx].start_pc;
		});

	// Since the list is sorted by start_pc, the scopes with the highest start_pc
	// (most specific) are closest to our iterator.
	while (it != sorted_scopes.begin())
	{
		--it;
		const auto& s = scopes[*it];
		if (pc <= s.end_pc)
			return &s;
	}

	return nullptr;
}

const DebugScope* DebugData::resolveFunctionScope(pc_t pc) const
{
	auto* scope = resolveScope(pc);
	if (!scope)
		return nullptr;

	while (scope->tag != TAG_FUNCTION)
	{
		if (scope->parent_index == -1)
			return nullptr;

		scope = &scopes[scope->parent_index];
	}

	return scope;
}

const DebugScope* DebugData::resolveFileScope(std::string fname) const
{
	for (const auto& s : scopes)
	{
		if (s.tag == TAG_FILE && s.name == fname)
			return &s;
	}

	return nullptr;
}

const DebugType* DebugData::getType(uint32_t type_id) const
{
	int32_t table_idx = type_id - DEBUG_TYPE_TAG_TABLE_START;
	if (table_idx < 0 || table_idx >= types.size()) return nullptr;

	return &types[table_idx];
}

std::string DebugData::getTypeName(uint32_t type_id) const
{
	switch (type_id)
	{
		case TYPE_VOID: return "void";
		case TYPE_UNTYPED: return "untyped";
		case TYPE_TEMPLATE_UNBOUNDED: return "T";
		case TYPE_BOOL: return "bool";
		case TYPE_INT: return "int";
		case TYPE_LONG: return "long";
		case TYPE_CHAR32: return "char32";
		case TYPE_RGB: return "rgb";
	}

	uint32_t table_idx = type_id - DEBUG_TYPE_TAG_TABLE_START;
	if (table_idx < 0 || table_idx >= types.size()) return "unknown";

	const DebugType& t = types[table_idx];

	switch (t.tag)
	{
		case TYPE_CONST:
			return "const " + getTypeName(t.extra);
		case TYPE_ARRAY:
			return getTypeName(t.extra) + "[]";
		case TYPE_BITFLAGS:
		case TYPE_CLASS:
		case TYPE_ENUM:
			return scopes[t.extra].name;
		default:
			return getTypeName(t.extra);
	}

	return "unknown";
}

std::string DebugData::getFullScopeName(const DebugScope* scope) const
{
	if (scope->tag == TAG_ROOT || scope->tag == TAG_FILE)
		return scope->name;

	std::vector<std::string> parts;

	const DebugScope* cur = scope;
	while (cur)
	{
		if (cur->tag == TAG_FUNCTION || cur->tag == TAG_CLASS || cur->tag == TAG_SCRIPT || cur->tag == TAG_NAMESPACE)
			parts.push_back(cur->name.empty() ? "?" : cur->name);

		if (cur->parent_index != -1)
			cur = &scopes[cur->parent_index];
		else
			cur = nullptr;
	}

	std::reverse(parts.begin(), parts.end());

	return fmt::format("{}", fmt::join(parts, "::"));
}

std::string DebugData::getFunctionSignature(const DebugScope* scope) const
{
	if (scope->tag != TAG_FUNCTION)
		return "";

	std::string name = getFullScopeName(scope);

	std::vector<std::string> params;
	for (auto symbol : getChildSymbols(scope))
	{
		if (symbol->flags & SYM_FLAG_HIDDEN)
			continue;

		bool varargs = symbol->flags & SYM_FLAG_VARARGS;
		std::string type_name = getTypeName(symbol->type_id);
		params.push_back(fmt::format("{}{} {}", varargs ? "..." : "", type_name, symbol->name));
	}

	std::string type_name = getTypeName(scope->type_id);
	return fmt::format("{} {}({})", type_name, name, fmt::join(params, ", "));
}

std::string DebugData::getDebugSymbolName(const DebugSymbol* symbol) const
{
	auto format_storage = [](DebugSymbolStorage s, int32_t offset, uint32_t type_id) -> std::string {
		switch(s) {
			case CONSTANT:
			{
				if (type_id == TYPE_INT)
					return fmt::format("Constant[{}]", offset / 10000);
				if (type_id == TYPE_LONG)
					return fmt::format("Constant[{}L]", offset);
				return fmt::format("Constant[{}]", offset);
			}
			case LOC_STACK:    return fmt::format("Stack[{}]", offset);
			case LOC_GLOBAL:   return fmt::format("Global[{}]", offset);
			case LOC_REGISTER: return fmt::format("Reg[{}]", get_script_variable(offset).first->name);
			case LOC_CLASS:    return fmt::format("ClassField[{}]", offset);
			default:           return fmt::format("Unknown[{}]", offset);
		}
	};

	std::string extra_info;
	if (symbol->flags & SYM_FLAG_HIDDEN)
		extra_info += " <HIDDEN>";
	if (symbol->flags & SYM_FLAG_VARARGS)
		extra_info += " <VARARG>";

	return fmt::format("{} {} @ {}{}",
		getTypeName(symbol->type_id),
		symbol->name,
		format_storage(symbol->storage, symbol->offset, symbol->type_id),
		extra_info);
}

void DebugData::buildSymbolCache() const
{
	if (scope_symbol_cache_built) return;
	scope_symbol_cache.resize(scopes.size());
	for (const auto& sym : symbols) {
		if (sym.scope_index >= 0 && sym.scope_index < scopes.size()) {
			scope_symbol_cache[sym.scope_index].push_back(&sym);
		}
	}
	scope_symbol_cache_built = true;
}

std::vector<const DebugSymbol*> DebugData::getChildSymbols(const DebugScope* scope) const
{
	buildSymbolCache();

	int32_t s_idx = (int32_t)(scope - &scopes[0]);
	return scope_symbol_cache[s_idx];
}

std::vector<const DebugScope*> DebugData::getChildScopes(const DebugScope* scope) const
{
	buildScopeChildrenCache();

	int32_t s_idx = (int32_t)(scope - &scopes[0]);
	auto& child_indices = scope_children_cache[s_idx];

	std::vector<const DebugScope*> child_scopes;
	for (auto index : child_indices)
		child_scopes.push_back(&scopes[index]);
	return child_scopes;
}

void DebugData::buildScopeChildrenCache() const
{
	if (scope_children_cache_built)
		return;

	scope_children_cache.resize(scopes.size());

	for (int i = 0; i < scopes.size(); i++)
	{
		int p = scopes[i].parent_index;
		if (p >= 0 && p < scopes.size())
			scope_children_cache[p].push_back(i);
	}

	scope_children_cache_built = true;
}

DebugData::ResolveResult DebugData::resolveEntity(const std::string& identifier, const DebugScope* current_scope) const
{
	buildSymbolCache();
	buildScopeChildrenCache();

	auto tokens = split_identifier(identifier);
	if (tokens.empty()) return {};

	ResolveResult current_res;
	int32_t ctx_scope_idx = current_scope ? (int32_t)(current_scope - &scopes[0]) : -1;

	// Only used within find_member, but declared here to avoid allocating a ton.
	std::vector<int32_t> search_queue;
	std::vector<int32_t> visited;
	search_queue.reserve(16);
	visited.reserve(16);

	// Resolve symbols and scopes within a specific scope.
	auto find_member = [&](int32_t parent_idx, std::string_view name) -> ResolveResult 
	{
		ResolveResult res;

		search_queue.clear();
		visited.clear();
		search_queue.push_back(parent_idx);

		size_t head = 0;
		while(head < search_queue.size())
		{
			int32_t s_idx = search_queue[head++];

			bool already_visited = false;
			for (int32_t v : visited)
			{
				if (v == s_idx)
				{
					already_visited = true;
					break;
				}
			}
			if (already_visited) continue;
			visited.push_back(s_idx);

			// 1. Check Symbols
			if (s_idx < scope_symbol_cache.size()) {
				for (const auto* sym : scope_symbol_cache[s_idx]) {
					if (sym->name == name) {
						res.sym = sym;
						break;
					}
				}
			}

			// 2. Check Child Scopes
			if (s_idx < scope_children_cache.size()) {
				for (int32_t child_idx : scope_children_cache[s_idx]) {
					const auto& child = scopes[child_idx];
					if (child.tag == TAG_FILE) continue;

					if (child.name == name) {
						res.scope_idx = child_idx;
						break;
					}
				}
			}

			// If we found ANYTHING at this level (Symbol OR Scope), return.
			// This ensures we respect shadowing (Derived matches hide Base matches).
			if (res.sym || res.scope_idx != -1)
				return res;

			// Check inheritance (base class).
			if (scopes[s_idx].tag == TAG_CLASS && scopes[s_idx].inheritance_index != -1)
				search_queue.push_back(scopes[s_idx].inheritance_index);
		}
		return res;
	};

	// =========================================================
	// Phase 1: Context Walk (Start Point)
	// =========================================================
	int token_idx = 0;

	if (tokens[0].empty())
	{
		// "::Global" syntax not supported.
		return {};
	}
	else
	{
		// Walk up parents + imports.
		const DebugScope* walker = (ctx_scope_idx >= 0) ? &scopes[ctx_scope_idx] : nullptr;
		bool found = false;

		while (walker)
		{
			int32_t s_idx = (int32_t)(walker - &scopes[0]);
			
			// Collect scan targets: Self + Imports
			std::vector<int32_t> lookups = { s_idx };
			lookups.insert(lookups.end(), walker->imports.begin(), walker->imports.end());

			for (int32_t lookup_idx : lookups) 
			{
				current_res = find_member(lookup_idx, tokens[0]);
				if (current_res.sym || current_res.scope_idx != -1)
				{
					found = true;
					break;
				}
			}

			if (found)
				break;

			if (walker->parent_index != -1)
				walker = &scopes[walker->parent_index];
			else 
				walker = nullptr;
		}

		if (!found)
			return {};

		token_idx++;
	}

	// =========================================================
	// Phase 2: Drill Down
	// =========================================================
	while (token_idx < tokens.size())
	{
		if (current_res.scope_idx == -1)
			return {};

		current_res = find_member(current_res.scope_idx, tokens[token_idx]);
		if (!current_res.sym && current_res.scope_idx == -1)
			return {};

		token_idx++;
	}

	return current_res;
}

const DebugSymbol* DebugData::resolveSymbol(const std::string& identifier, const DebugScope* current_scope) const
{
	ResolveResult res = resolveEntity(identifier, current_scope);
	return res.sym;
}

const DebugScope* DebugData::resolveScope(const std::string& identifier, const DebugScope* current_scope) const
{
	ResolveResult res = resolveEntity(identifier, current_scope);
	if (res.scope_idx != -1)
		return &scopes[res.scope_idx];
	return nullptr;
}

std::vector<const DebugScope*> DebugData::resolveFunctions(const std::string& identifier, const DebugScope* current_scope) const
{
	buildSymbolCache();
	buildScopeChildrenCache();

	auto tokens = split_identifier(identifier);
	if (tokens.empty()) return {};

	std::vector<const DebugScope*> candidates;
	
	// Helper: Collect ALL function scopes with 'name' inside 'parent_idx'
	// (including base classes)
	auto collect_functions_recursive = [&](int32_t start_idx, std::string_view name) 
	{
		std::vector<int32_t> search_queue = { start_idx };
		std::unordered_set<int32_t> visited;

		size_t head = 0;
		while(head < search_queue.size())
		{
			int32_t s_idx = search_queue[head++];
			if (visited.count(s_idx)) continue;
			visited.insert(s_idx);

			// Check Child Scopes for FUNCTIONS
			if (s_idx < scope_children_cache.size()) {
				for (int32_t child_idx : scope_children_cache[s_idx]) {
					const auto& child = scopes[child_idx];
					
					// We only care about matching names that are FUNCTION scopes
					if (child.name == name && child.tag == TAG_FUNCTION) {
						candidates.push_back(&child);
					}
				}
			}

			// Check Inheritance (Base Class)
			if (scopes[s_idx].tag == TAG_CLASS && scopes[s_idx].inheritance_index != -1)
				search_queue.push_back(scopes[s_idx].inheritance_index);
		}
	};

	// =========================================================
	// Logic: Walk up until we find ANY match, then stop ascending
	// =========================================================
	
	// 1. If the identifier is simple ("MyFunc"), we walk up the stack.
	if (tokens.size() == 1)
	{
		const DebugScope* walker = current_scope;
		int32_t ctx_idx = current_scope ? (int32_t)(current_scope - &scopes[0]) : -1;

		while (ctx_idx != -1)
		{
			const auto& scope = scopes[ctx_idx];
			size_t count_before = candidates.size();

			// A. Check Self
			collect_functions_recursive(ctx_idx, tokens[0]);

			// B. Check Imports (using namespace, etc)
			for (int32_t import_idx : scope.imports) {
				collect_functions_recursive(import_idx, tokens[0]);
			}

			// STOPPING CONDITION:
			// Standard C++ rules: if we found candidates in this scope (or its imports),
			// we stop walking up. We don't merge "local functions" with "global functions".
			// The local ones shadow the globals.
			if (candidates.size() > count_before)
				return candidates;

			ctx_idx = scope.parent_index;
		}
		return candidates; // Might be empty if nothing found anywhere
	}
	
	// 2. If the identifier is complex ("MyClass::MyFunc"), we resolve the parent first.
	else
	{
		// A. Resolve the container (e.g., "MyClass") using standard rules
		// We use the existing resolveScope to find the container.
		// We construct the parent path by removing the last token.
		std::string parent_path = identifier.substr(0, identifier.rfind(tokens.back()) - 2); 
		// (Note: simple string logic above implies '::' separator length is 2. 
		//  Better to join tokens[0]...tokens[n-1])

		const DebugScope* container = resolveScope(parent_path, current_scope);
		if (!container) return {};

		int32_t container_idx = (int32_t)(container - &scopes[0]);
		
		// B. Collect functions inside that container
		collect_functions_recursive(container_idx, tokens.back());
		
		return candidates;
	}
}

std::optional<DebugData> DebugData::decode(const std::vector<byte>& buffer)
{
	if (buffer.empty())
		return std::nullopt;

	DebugData result{};
	size_t cursor = 0;

	uint32_t version = read_uvlq_from_buffer(buffer, cursor);
	if (version != 1)
		return std::nullopt;

	// Source files.
	uint32_t file_count = read_uvlq_from_buffer(buffer, cursor);
	result.source_files.reserve(file_count);
	for (uint32_t i = 0; i < file_count; ++i)
	{
		SourceFile file;

		// Path.
		uint32_t path_len = read_uvlq_from_buffer(buffer, cursor);
		if (cursor + path_len > buffer.size()) return std::nullopt;
		file.path.assign(reinterpret_cast<const char*>(&buffer[cursor]), path_len);
		cursor += path_len;

		// Contents.
		uint32_t contents_len = read_uvlq_from_buffer(buffer, cursor);
		if (cursor + contents_len > buffer.size()) return std::nullopt;
		file.contents.assign(reinterpret_cast<const char*>(&buffer[cursor]), contents_len);
		cursor += contents_len;

		result.source_files.push_back(std::move(file));
	}

	// Debug lines (encoded ops).
	uint32_t debug_lines_size = read_uvlq_from_buffer(buffer, cursor);
	if (buffer.size() - cursor < debug_lines_size)
		return std::nullopt;

	result.debug_lines_encoded.assign(buffer.begin() + cursor, buffer.begin() + cursor + debug_lines_size);
	cursor += debug_lines_size;

	// Types.
	uint32_t count = read_uvlq_from_buffer(buffer, cursor);
	result.types.resize(count);
	for (uint32_t i = 0; i < count; i++)
	{
		result.types[i].tag = (DebugTypeTag)read_uvlq_from_buffer(buffer, cursor);
		result.types[i].extra = read_svlq_from_buffer(buffer, cursor);
	}

	// Scopes.
	count = read_uvlq_from_buffer(buffer, cursor);
	result.scopes.reserve(count);
	for (uint32_t i = 0; i < count; i++)
	{
		DebugScope scope{};
		scope.tag = (DebugScopeTag)read_uvlq_from_buffer(buffer, cursor);
		scope.flags = (DebugScopeFlags)read_uvlq_from_buffer(buffer, cursor);
		scope.parent_index = read_svlq_from_buffer(buffer, cursor);

		if (scope.tag == TAG_CLASS)
			scope.inheritance_index = read_svlq_from_buffer(buffer, cursor);

		uint32_t imported_scopes_len = read_uvlq_from_buffer(buffer, cursor);
		if (cursor + imported_scopes_len > buffer.size()) return std::nullopt;
		for (uint32_t j = 0; j < imported_scopes_len; j++)
			scope.imports.push_back(read_uvlq_from_buffer(buffer, cursor));

		if (scope.tag == TAG_BLOCK || scope.tag == TAG_FUNCTION)
		{
			scope.start_pc = read_uvlq_from_buffer(buffer, cursor);
			scope.end_pc = read_uvlq_from_buffer(buffer, cursor);
		}

		if (scope.tag != TAG_BLOCK)
		{
			uint32_t name_len = read_uvlq_from_buffer(buffer, cursor);
			if (cursor + name_len > buffer.size()) return std::nullopt;
			scope.name.assign((const char*)&buffer[cursor], name_len);
			cursor += name_len;

			scope.type_id = read_uvlq_from_buffer(buffer, cursor);
		}

		result.scopes.push_back(std::move(scope));
	}

	// Symbols.
	count = read_uvlq_from_buffer(buffer, cursor);
	result.symbols.reserve(count);
	for (uint32_t i = 0; i < count; i++)
	{
		DebugSymbol symbol{};
		symbol.scope_index = read_svlq_from_buffer(buffer, cursor);
		symbol.offset = read_svlq_from_buffer(buffer, cursor); // Signed!
		symbol.type_id = read_uvlq_from_buffer(buffer, cursor);
		symbol.flags = (DebugSymbolFlags)read_uvlq_from_buffer(buffer, cursor);
		symbol.storage = (DebugSymbolStorage)read_uvlq_from_buffer(buffer, cursor);

		uint32_t name_len = read_uvlq_from_buffer(buffer, cursor);
		if (cursor + name_len > buffer.size()) return std::nullopt;
		symbol.name.assign((const char*)&buffer[cursor], name_len);
		cursor += name_len;

		result.symbols.push_back(std::move(symbol));
	}

	return result;
}

std::vector<byte> DebugData::encode() const
{
	std::vector<byte> buffer;
	buffer.reserve(debug_lines_encoded.size());

	// Version.
	write_unsigned_vlq(buffer, VERSION);

	// Source files.
	write_unsigned_vlq(buffer, source_files.size());
	for (const auto& file : source_files)
	{
		// Path.
		write_unsigned_vlq(buffer, file.path.size());
		const byte* path_ptr = reinterpret_cast<const byte*>(file.path.data());
		buffer.insert(buffer.end(), path_ptr, path_ptr + file.path.size());

		// Contents.
		write_unsigned_vlq(buffer, file.contents.size());
		const byte* contents_ptr = reinterpret_cast<const byte*>(file.contents.data());
		buffer.insert(buffer.end(), contents_ptr, contents_ptr + file.contents.size());
	}

	// Debug lines (encoded ops).
	write_unsigned_vlq(buffer, debug_lines_encoded.size());
	buffer.insert(buffer.end(), debug_lines_encoded.begin(), debug_lines_encoded.end());

	// Types.
	write_unsigned_vlq(buffer, types.size());
	for (const auto& t : types)
	{
		write_unsigned_vlq(buffer, t.tag);
		write_signed_vlq(buffer, t.extra);
	}

	// Scopes.
	write_unsigned_vlq(buffer, scopes.size());
	for (const auto& scope : scopes)
	{
		write_unsigned_vlq(buffer, scope.tag);
		write_unsigned_vlq(buffer, scope.flags);
		write_signed_vlq(buffer, scope.parent_index);

		if (scope.tag == TAG_CLASS)
			write_signed_vlq(buffer, scope.inheritance_index);

		write_unsigned_vlq(buffer, scope.imports.size());
		for (const auto& index : scope.imports)
			write_unsigned_vlq(buffer, index);

		if (scope.tag == TAG_BLOCK || scope.tag == TAG_FUNCTION)
		{
			write_unsigned_vlq(buffer, scope.start_pc);
			write_unsigned_vlq(buffer, scope.end_pc);
		}

		if (scope.tag != TAG_BLOCK)
		{
			write_unsigned_vlq(buffer, scope.name.size());
			buffer.insert(buffer.end(), scope.name.begin(), scope.name.end());
	
			write_unsigned_vlq(buffer, scope.type_id);
		}
	}

	// Symbols.
	write_unsigned_vlq(buffer, symbols.size());
	for (const auto& symbol : symbols)
	{
		write_signed_vlq(buffer, symbol.scope_index);
		write_signed_vlq(buffer, symbol.offset);
		write_unsigned_vlq(buffer, symbol.type_id);
		write_unsigned_vlq(buffer, symbol.flags);
		write_unsigned_vlq(buffer, symbol.storage);

		write_unsigned_vlq(buffer, symbol.name.size());
		buffer.insert(buffer.end(), symbol.name.begin(), symbol.name.end());
	}

	return buffer;
}

// Builds a skip-list snapshot of the debug lines by caching every Nth position, requiring
// resolveLocation to process only a small slice of the encoded data stream.
void DebugData::buildCheckpoints() const
{
	const int N = 100;

	// Reserve estimation: assuming avg 2 bytes per instruction.
	checkpoints.clear();
	checkpoints.reserve(debug_lines_encoded.size() / N / 2);

	size_t cursor = 0;
	int32_t current_line = 1;
	int32_t current_file = 0;
	pc_t current_pc = 0;
	size_t next_checkpoint_pc = 0;

	// Always add the zero checkpoint.
	checkpoints.push_back({0, 0, 1, 0});
	next_checkpoint_pc += N;

	while (cursor < debug_lines_encoded.size())
	{
		// Store checkpoint.
		if (current_pc >= next_checkpoint_pc)
		{
			checkpoints.push_back({current_pc, cursor, current_line, current_file});
			next_checkpoint_pc += N;
		}

		byte cmd = debug_lines_encoded[cursor++];

		if (cmd == DEBUG_LINE_OP_SET_FILE)
		{
			current_file = read_uvlq_from_buffer(debug_lines_encoded, cursor);
			continue;
		}

		size_t d_pc;
		int32_t d_line;

		if (cmd == DEBUG_LINE_OP_EXTENDED_STEP)
		{
			d_pc = read_uvlq_from_buffer(debug_lines_encoded, cursor);
			d_line = read_svlq_from_buffer(debug_lines_encoded, cursor);
		}
		else
		{
			d_pc = cmd;
			d_line = 1;
		}

		current_pc += d_pc;
		current_line += d_line;
	}

	checkpoints_built = true;
}

std::string DebugData::internalToStringForDebugging() const
{
	std::stringstream ss;

	// Build adjacency list.
	std::map<int32_t, std::vector<int32_t>> scope_children;
	std::vector<int32_t> root_indices;

	for (size_t i = 0; i < scopes.size(); ++i)
	{
		if (scopes[i].parent_index == -1)
			root_indices.push_back((int32_t)i);
		else
			scope_children[scopes[i].parent_index].push_back((int32_t)i);
	}

	// Map symbols to scopes.
	std::map<int32_t, std::vector<const DebugSymbol*>> scope_symbols;
	for (const auto& sym : symbols)
	{
		scope_symbols[sym.scope_index].push_back(&sym);
	}

	auto get_tag_name = [](DebugScopeTag t) -> const char* {
		switch(t) {
			case TAG_ROOT:       return "ROOT";
			case TAG_NAMESPACE:  return "NAMESPACE";
			case TAG_SCRIPT:     return "SCRIPT";
			case TAG_FUNCTION:   return "FUNCTION";
			case TAG_CLASS:      return "CLASS";
			case TAG_ENUM:       return "ENUM";
			case TAG_BLOCK:      return "BLOCK";
			case TAG_FILE:       return "FILE";
			default:             return "UNKNOWN";
		}
	};

	std::function<void(int32_t, int)> print_scope = [&](int32_t scope_idx, int indent) {
		// Make the header text.
		// Example: [FUNCTION] void MyFunc <PC:100-200>

		const auto& scope = scopes[scope_idx];
		std::string pad(indent * 2, ' ');
		std::string name = scope.tag == TAG_BLOCK ?
			"" :
			scope.name.empty() ? "<anon>" : scope.name;

		std::vector<std::string> parts;
		parts.push_back(fmt::format("[{}]", get_tag_name(scope.tag)));
		if (scope.type_id != TYPE_VOID)
			parts.push_back(getTypeName(scope.type_id));
		parts.push_back(name);
		if (scope.inheritance_index >= 0 && scope.inheritance_index < (int)scopes.size())
			parts.push_back(fmt::format(" extends {}", scopes[scope.inheritance_index].name));

		if (scope.tag == TAG_FUNCTION || scope.tag == TAG_BLOCK)
			parts.push_back(fmt::format("<PC:{}-{} IDX:{}>", scope.start_pc, scope.end_pc, scope_idx));
		else
			parts.push_back(fmt::format("<IDX:{}>", scope_idx));

		if (scope.flags & SCOPE_FLAG_HIDDEN)
			parts.push_back("<HIDDEN>");
		if (scope.flags & SCOPE_FLAG_INTERNAL)
			parts.push_back("<INTERNAL>");
		if (scope.flags & SCOPE_FLAG_DEPRECATED)
			parts.push_back("<DEPRECATED>");

		std::erase_if(parts, [](auto& v){
			return v.empty();
		});
		ss << fmt::format("{}{}\n", pad, fmt::join(parts, " "));

		if (scope_symbols.count(scope_idx))
		{
			for (const auto* sym : scope_symbols[scope_idx])
			{
				std::string extra_info;
				if (sym->flags & SYM_FLAG_HIDDEN)
					extra_info += " <HIDDEN>";
				if (sym->flags & SYM_FLAG_VARARGS)
					extra_info += " <VARARG>";

				std::string type_name = getTypeName(sym->type_id);

				// Example: - int myVar @ Stack[4]
				ss << fmt::format("{}  - {}\n", pad, getDebugSymbolName(sym));
			}
		}

		if (scope_children.count(scope_idx))
		{
			for (int32_t child_idx : scope_children[scope_idx])
			{
				print_scope(child_idx, indent + 1);
			}
		}
	};

	if (root_indices.empty() && !scopes.empty())
	{
		ss << "<Error: No Root Scopes Found>\n";
	}

	ss << "Scopes:\n\n";
	for (int32_t root : root_indices)
	{
		print_scope(root, 0);
	}

	// Types.
	ss << "\nTypes:\n\n";
	for (int i = 0; i < types.size() + DEBUG_TYPE_TAG_TABLE_START; i++)
		ss << i << ": " << getTypeName(i) << '\n';

	return ss.str();
}
