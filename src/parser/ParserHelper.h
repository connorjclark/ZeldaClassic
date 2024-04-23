#ifndef ZC_PARSER_HELPER_H
#define ZC_PARSER_HELPER_H

#include "parser/AST.h"

using namespace ZScript;

namespace ParserHelper {

ParserScriptType getScriptType(std::string name);

bool isValidIdentifier(std::string identifier);

}

#endif
