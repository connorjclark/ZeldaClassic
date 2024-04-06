#include "base/util.h"
#include "parser/AST.h"
#include "parserDefs.h"
#include "MetadataVisitor.h"
#include <cassert>
#include "Scope.h"
#include "CompileError.h"
#include <nlohmann/json.hpp>

using namespace ZScript;
using json = nlohmann::ordered_json;

json root;
json* active;

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#symbolKind
enum class SymbolKind
{
	File = 1,
	Module,
	Namespace,
	Package,
	Class,
	Method,
	Property,
	Field,
	Constructor,
	Enum,
	Interface,
	Function,
	Variable,
	Constant,
	String,
	Number,
	Boolean,
	Array,
	Object,
	Key,
	Null,
	EnumMember,
	Struct,
	Event,
	Operator,
	TypeParameter,
};

template <typename T>
static std::string getName(const T& node)
{
	return node.name;
}
static std::string getName(const ASTFile& node)
{
	return node.scope->getName().value_or("<file>");
}
static std::string getName(const ASTScript& node)
{
	return node.script->getName();
}

template <typename T>
static LocationData getSelectionRange(const T& node)
{
	return node.location;
}
static LocationData getSelectionRange(const ASTScript& node)
{
	return node.name_location;
}
static LocationData getSelectionRange(const ASTClass& node)
{
	return node.name_location;
}

static auto LocationData_json(const LocationData& loc)
{
	return json{
		{"start", {
			{"line", loc.first_line - 1}, {"character", loc.first_column - 1},
		}},
		{"end", {
			{"line", loc.last_line - 1}, {"character", loc.last_column - 1},
		}}
	};
}

static auto LocationData_pos_json(const LocationData& loc)
{
	assert(loc.first_line == loc.last_line);
	return json{
		{"line", loc.first_line - 1}, {"character", loc.first_column - 1}, {"length", loc.last_column - loc.first_column},
	};
}

template <typename T>
static void appendDocSymbol(SymbolKind kind, const T& node)
{
	(*active).push_back({
		{"name", getName(node)},
		{"kind", kind},
		{"range", LocationData_json(node.location)},
		{"selectionRange", LocationData_json(getSelectionRange(node))},
		{"children", json::array()},
	});
	active = &(*active).back()["children"];
}

static std::string cleanComment(std::string comment)
{
	if (comment.empty())
		return comment;

	std::string copy = comment;
	util::trimstr(copy);
	return copy;
}

template <typename T>
static void appendIdentifier(const T& node, const LocationData& loc)
{
	if (!node.binding || !node.binding->getNode())
		return;

	auto key = std::to_string(node.binding->id);
	if (!root["symbols"].contains(key))
	{
		root["symbols"][key] = {
			// TODO LocationData_location_json
			{"loc", {
				{"range", LocationData_json(node.binding->getNode()->location)},
				{"uri", node.binding->getNode()->location.fname},
			}},
		};

		auto comment = cleanComment(node.binding->getNode()->doc_comment);
		if (!comment.empty())
			root["symbols"][key]["doc"] = comment;
	}

	root["identifiers"].push_back({
		// {"name", node.binding->getName()}, // TODO ! remove
		{"loc", LocationData_pos_json(loc)},
		{"symbol", node.binding->id},
	});
}

MetadataVisitor::MetadataVisitor(Program& program)
	: program(program)
{
	root = {
		{"currentFileSymbols", json::array()},
		{"symbols",  json::object()},
		{"identifiers",  json::array()},
	};
	active = &root["currentFileSymbols"];
	visit(program.getRoot());
}

void MetadataVisitor::visit(AST& node, void* param)
{
	RecursiveVisitor::visit(node, param);
}

void MetadataVisitor::caseFile(ASTFile& host, void* param)
{
	auto name = getName(host);
	if (name != "ZQ_BUFFER" && !name.ends_with("tmp.zs"))
		return;

	RecursiveVisitor::caseFile(host, param);
}

void MetadataVisitor::caseNamespace(ASTNamespace& host, void* param)
{
	auto prev_active = active;
	appendDocSymbol(SymbolKind::Namespace, host);
	RecursiveVisitor::caseNamespace(host, param);
	active = prev_active;
}

void MetadataVisitor::caseScript(ASTScript& host, void* param)
{
	auto prev_active = active;
	appendDocSymbol(SymbolKind::Module, host);
	RecursiveVisitor::caseScript(host, param);
	active = prev_active;
}

void MetadataVisitor::caseClass(ASTClass& host, void* param)
{
	auto prev_active = active;
	appendDocSymbol(SymbolKind::Class, host);
	RecursiveVisitor::caseClass(host, param);
	active = prev_active;
}

void MetadataVisitor::caseDataDecl(ASTDataDecl& host, void* param)
{
	auto prev_active = active;
	appendDocSymbol(SymbolKind::Variable, host);
	RecursiveVisitor::caseDataDecl(host, param);
	active = prev_active;
}

void MetadataVisitor::caseFuncDecl(ASTFuncDecl& host, void* param)
{
	if (host.prototype)
		return;

	auto prev_active = active;
	auto kind = host.getFlag(FUNCFLAG_CONSTRUCTOR) ? SymbolKind::Constructor : SymbolKind::Function;
	appendDocSymbol(kind, host);
	if (!host.prototype)
		visit(host.block.get(), param);
	active = prev_active;
}

void MetadataVisitor::caseExprIdentifier(ASTExprIdentifier& host, void* param)
{
	// TODO: create identifiers for namespace components
	if (!host.isConstant() && host.binding && !host.binding->isBuiltIn())
		appendIdentifier(host, host.componentNodes.back()->location);

	RecursiveVisitor::caseExprIdentifier(host, param);
}

void MetadataVisitor::caseExprArrow(ASTExprArrow& host, void* param)
{
	// TODO: need to enrich ASTExprArrow with a binding.
	// appendIdentifier(host, host.right);
	RecursiveVisitor::caseExprArrow(host, param);
}

void MetadataVisitor::caseExprCall(ASTExprCall& host, void* param)
{
	// TODO: create identifiers for namespace components
	if (auto expr_ident = dynamic_cast<ASTExprIdentifier*>(host.left.get()))
		appendIdentifier(host, expr_ident->componentNodes.back()->location);
	RecursiveVisitor::caseExprCall(host, param);
}

std::string MetadataVisitor::getOutput()
{
	return root.dump(2);
}
