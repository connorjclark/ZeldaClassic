#ifndef COMMENT_UTILS_H_
#define COMMENT_UTILS_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ZScript {
	class AST;
}

// Parses into a map keyed by annotations (ex: @alias).
// Duplicate annotations have multiple entries.
// `description` contains all non-annotation text.
struct ParsedComment {
	ParsedComment(const std::string& comment);

	bool contains_annotation(const std::string& key) const;
	std::optional<std::string> get_annotation(const std::string& key) const;
	std::vector<std::string> get_multi_annotation(const std::string& key) const;

	std::vector<std::pair<std::string, std::string>> annotations;
	std::string description;
};

uint16_t getSymbolId(const ZScript::AST* node);

struct CommentSymbolParseResult {
	std::string symbol_name;
	int pos;
	int len;
	std::string link_text;
	const ZScript::AST* symbol_node;
};

std::vector<CommentSymbolParseResult> parseForSymbolLinks(std::string comment, const ZScript::AST* node, bool check_params);

#endif
