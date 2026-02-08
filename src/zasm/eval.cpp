// This is only used for the debugger.

#include "zasm/eval.h"
#include "zasm/debug_data.h"
#include <cctype>
#include <stdexcept>

ExpressionParser::ExpressionParser(std::string s) : input(s)
{
}

char ExpressionParser::peek()
{
	return pos < input.size() ? input[pos] : 0;
}

char ExpressionParser::get()
{
	return pos < input.size() ? input[pos++] : 0;
}

void ExpressionParser::skipWhitespace()
{
	while (isspace(peek()))
		pos++;
}

bool ExpressionParser::match(char c)
{
	skipWhitespace();
	if (peek() == c)
	{
		pos++;
		return true;
	}
	return false;
}

std::shared_ptr<ExprNode> ExpressionParser::parseExpression()
{
	return parseLogicalOr();
}

std::shared_ptr<ExprNode> ExpressionParser::parseLogicalOr()
{
	auto left = parseLogicalAnd();
	skipWhitespace();
	while (peek() == '|' && pos + 1 < input.size() && input[pos + 1] == '|')
	{
		pos += 2; // Consume ||
		auto right = parseLogicalAnd();
		left = std::make_shared<BinaryOpNode>("||", left, right);
	}
	return left;
}

std::shared_ptr<ExprNode> ExpressionParser::parseLogicalAnd()
{
	auto left = parseBitOr();
	skipWhitespace();
	while (peek() == '&' && pos + 1 < input.size() && input[pos + 1] == '&')
	{
		pos += 2; // Consume &&
		auto right = parseBitOr();
		left = std::make_shared<BinaryOpNode>("&&", left, right);
	}
	return left;
}

std::shared_ptr<ExprNode> ExpressionParser::parseBitOr()
{
	auto left = parseBitXor();
	skipWhitespace();
	while (peek() == '|')
	{
		// If next char is |, it's a Logical OR, let the parent handle it
		if (pos + 1 < input.size() && input[pos + 1] == '|')
			break;

		std::string op(1, get());
		auto right = parseBitXor();
		left = std::make_shared<BinaryOpNode>(op, left, right);
	}
	return left;
}

std::shared_ptr<ExprNode> ExpressionParser::parseBitXor()
{
	auto left = parseBitAnd();
	skipWhitespace();
	while (peek() == '^')
	{
		std::string op(1, get());
		auto right = parseBitAnd();
		left = std::make_shared<BinaryOpNode>(op, left, right);
	}
	return left;
}

std::shared_ptr<ExprNode> ExpressionParser::parseBitAnd()
{
	auto left = parseEquality();
	skipWhitespace();
	while (peek() == '&')
	{
		// If next char is &, it's a Logical AND, let the parent handle it
		if (pos + 1 < input.size() && input[pos + 1] == '&')
			break;

		std::string op(1, get());
		auto right = parseEquality();
		left = std::make_shared<BinaryOpNode>(op, left, right);
	}
	return left;
}

std::shared_ptr<ExprNode> ExpressionParser::parseEquality()
{
	auto left = parseRelational();
	skipWhitespace();
	while (peek() == '=' || peek() == '!')
	{
		// Check for '==' or '!='
		if (peek() == '=' && pos + 1 < input.size() && input[pos + 1] == '=')
		{
			pos += 2;
			auto right = parseRelational();
			left = std::make_shared<BinaryOpNode>("==", left, right);
		}
		else if (peek() == '!' && pos + 1 < input.size() && input[pos + 1] == '=')
		{
			pos += 2;
			auto right = parseRelational();
			left = std::make_shared<BinaryOpNode>("!=", left, right);
		}
		else
		{
			break;
		}
	}
	return left;
}

std::shared_ptr<ExprNode> ExpressionParser::parseRelational()
{
	auto left = parseAdd();
	skipWhitespace();
	while (peek() == '<' || peek() == '>')
	{
		char first = get(); // Consume < or >
		std::string op(1, first);

		// Check for = (<= or >=)
		if (peek() == '=')
		{
			get();
			op += '=';
		}

		auto right = parseAdd();
		left = std::make_shared<BinaryOpNode>(op, left, right);
	}
	return left;
}

std::shared_ptr<ExprNode> ExpressionParser::parseAdd()
{
	auto left = parseFactor();
	skipWhitespace();
	while (peek() == '+' || peek() == '-')
	{
		std::string op(1, get());
		auto right = parseFactor();
		left = std::make_shared<BinaryOpNode>(op, left, right);
	}
	return left;
}

std::shared_ptr<ExprNode> ExpressionParser::parseFactor()
{
	auto left = parseUnary();
	skipWhitespace();
	while (peek() == '*' || peek() == '/')
	{
		std::string op(1, get());
		auto right = parseUnary();
		left = std::make_shared<BinaryOpNode>(op, left, right);
	}
	return left;
}

std::shared_ptr<ExprNode> ExpressionParser::parseUnary()
{
	skipWhitespace();
	char c = peek();
	
	if (c == '~' || c == '-' || c == '!') 
	{
		std::string op(1, get()); // Consume operator
		auto right = parseUnary(); // Recursive (allows ~~x)
		return std::make_shared<UnaryOpNode>(op, right);
	}
	
	return parsePrimary();
}

std::shared_ptr<ExprNode> ExpressionParser::parsePrimary()
{
	skipWhitespace();
	std::shared_ptr<ExprNode> left;
	char c = peek();

	// 1. Term parsing
	if (isdigit(c))
	{
		size_t start = pos;
		while (isdigit(peek()))
			pos++;

		bool is_long = false;
		if (peek() == 'L')
		{
			is_long = true;
			pos++;
		}

		std::string numStr = input.substr(start, pos - start - (is_long ? 1 : 0));
		int32_t val = std::stoi(numStr);

		DebugValue dv;
		if (is_long)
		{
			dv.raw_value = val;
			dv.type_id = TYPE_LONG;
		}
		else
		{
			dv.raw_value = val * FIXED_ONE;
			dv.type_id = TYPE_INT;
		}
		left = std::make_shared<LiteralNode>(dv);
	}
	else if (match('('))
	{
		left = parseExpression();
		if (!match(')'))
			throw std::runtime_error("Expected ')'");
	}
	else if (isalpha(c) || c == '_')
	{
		size_t start = pos;
		while (isalnum(peek()) || peek() == '_' || peek() == ':')
			pos++;
		std::string id = input.substr(start, pos - start);

		if (id == "new")
		{
			skipWhitespace();
			size_t c_start = pos;
			while (isalnum(peek()) || peek() == '_' || peek() == ':')
				pos++;
			std::string className = input.substr(c_start, pos - c_start);
			match('(');
			match(')');
			left = std::make_shared<NewNode>(className);
		}
		else
		{
			left = std::make_shared<VarNode>(id);
		}
	}
	else
	{
		throw std::runtime_error("Unexpected character");
	}

	// 2. Postfix Loop (handle -> and () and [])
	while (true)
	{
		skipWhitespace();

		// Function Call
		if (match('('))
		{
			// Determine function name
			std::string funcName;
			if (auto v = dynamic_cast<VarNode*>(left.get()))
				funcName = v->identifier;
			else if (auto m = dynamic_cast<MemberAccessNode*>(left.get()))
				funcName = m->member_name;
			else
				throw std::runtime_error("Expression is not callable");

			std::vector<std::shared_ptr<ExprNode>> args;
			if (!match(')'))
			{
				do
				{
					args.push_back(parseExpression());
				} while (match(','));
				if (!match(')'))
					throw std::runtime_error("Expected ')'");
			}

			auto call = std::make_shared<FuncCallNode>(funcName, args);

			// If previous node was MemberAccess, set Context
			if (left->type == E_MEMBER)
			{
				auto memNode = static_cast<MemberAccessNode*>(left.get());
				call->object_context = memNode->object;
			}

			left = call;
		}
		// Member Access
		else if (peek() == '-' && input[pos + 1] == '>')
		{
			pos += 2;
			skipWhitespace();

			size_t start = pos;
			while (isalnum(peek()) || peek() == '_')
				pos++;
			if (pos == start)
				throw std::runtime_error("Expected member name");

			std::string member = input.substr(start, pos - start);
			left = std::make_shared<MemberAccessNode>(left, member);
		}
		// Array indexing
		else if (match('[')) 
		{
			auto indexExpr = parseExpression();
			if (!match(']'))
				throw std::runtime_error("Expected ']' after array index");
			left = std::make_shared<IndexNode>(left, indexExpr);
		}
		else
		{
			break;
		}
	}

	return left;
}

ExpressionEvaluator::ExpressionEvaluator(const DebugData& dd, const DebugScope* scope, VMInterface& v)
	: debugData(dd), currentScope(scope), vm(v)
{
}

DebugValue ExpressionEvaluator::evaluate(std::shared_ptr<ExprNode> node)
{
	if (!node)
		return {0, 0, false};

	switch (node->type)
	{
		case E_LITERAL:
		{
			return static_cast<LiteralNode*>(node.get())->value;
		}

		case E_VAR:
		{
			auto varNode = static_cast<VarNode*>(node.get());
			const DebugSymbol* sym = debugData.resolveSymbol(varNode->identifier, currentScope);
			if (!sym)
				throw std::runtime_error("Unknown variable: " + varNode->identifier);
			return readValue(sym);
		}

		case E_MEMBER:
		{
			auto mem = static_cast<MemberAccessNode*>(node.get());
			DebugValue objVal = evaluate(mem->object);

			const DebugScope* cls = getClassScope(objVal.type_id);
			if (!cls)
				throw std::runtime_error("Accessing member of non-class type");

			const DebugSymbol* sym = debugData.resolveSymbol(mem->member_name, cls);
			if (!sym)
				throw std::runtime_error("Member not found: " + mem->member_name);

			// Access Heap Member
			DebugValue val{};
			val.raw_value = vm.readObjectMember(objVal.raw_value, sym->offset);
			val.type_id = sym->type_id;
			val.is_lvalue = true;
			return val;
		}

		case E_BINARY:
		{
			auto bin = static_cast<BinaryOpNode*>(node.get());

			// Handle logical ops with short-circuiting.
			if (bin->op == "&&" || bin->op == "||")
			{
				DebugValue l = evaluate(bin->left);
				bool l_true = (l.raw_value != 0);

				// Helper to determine '1' based on Fixed Point rules
				auto getTrueVal = [&](bool isFixed) { return FIXED_ONE; };

				if (bin->op == "&&")
				{
					if (!l_true)
						return {0, TYPE_BOOL, false}; // Short-circuit false.

					DebugValue r = evaluate(bin->right);
					return { (r.raw_value != 0) ? FIXED_ONE : 0, TYPE_BOOL, false };
				}
				else
				{
					if (l_true)
						return { getTrueVal(l.isFixed()), TYPE_BOOL, false }; // Short-circuit true.

					DebugValue r = evaluate(bin->right);
					return { (r.raw_value != 0) ? FIXED_ONE : 0, TYPE_BOOL, false };
				}
			}

			if (bin->op == "==" || bin->op == "!=" ||
				bin->op == "<" || bin->op == "<=" ||
				bin->op == ">" || bin->op == ">=")
			{
				DebugValue l = evaluate(bin->left);
				DebugValue r = evaluate(bin->right);

				int64_t l_val = l.raw_value;
				int64_t r_val = r.raw_value;

				bool res = false;
				if (bin->op == "==") res = (l_val == r_val);
				else if (bin->op == "!=") res = (l_val != r_val);
				else if (bin->op == "<")  res = (l_val < r_val);
				else if (bin->op == "<=") res = (l_val <= r_val);
				else if (bin->op == ">")  res = (l_val > r_val);
				else if (bin->op == ">=") res = (l_val >= r_val);

				return { res ? FIXED_ONE : 0, TYPE_BOOL, false };
			}

			DebugValue l = evaluate(bin->left);
			DebugValue r = evaluate(bin->right);
			return evalBinaryOp(bin->op, l, r);
		}

		case E_UNARY:
		{
			auto u = static_cast<UnaryOpNode*>(node.get());
			DebugValue val = evaluate(u->operand);

			if (u->op == "-")
			{
				val.raw_value = -val.raw_value;
			}
			else if (u->op == "~")
			{
				val.raw_value = ~val.raw_value;
			}
			else if (u->op == "!")
			{
				val.raw_value = (val.raw_value == 0) ? FIXED_ONE : 0;
			}
			return val;
		}

		case E_CALL:
		{
			auto call = static_cast<FuncCallNode*>(node.get());

			// Evaluate Args
			std::vector<DebugValue> argValues;
			std::vector<int32_t> argTypes;

			// Handle 'this' context if method call
			if (call->object_context)
			{
				DebugValue objVal = evaluate(call->object_context);
			}

			for (auto& arg : call->args)
			{
				DebugValue v = evaluate(arg);
				argValues.push_back(v);
				argTypes.push_back(v.type_id);
			}

			// Find Function
			const DebugScope* searchScope = currentScope;
			if (call->object_context)
			{
				// (Re-eval for type only is inefficient but safe)
				DebugValue objVal = evaluate(call->object_context);
				searchScope = getClassScope(objVal.type_id);
			}

			const DebugScope* funcScope = resolveOverload(call->name, argTypes, searchScope);
			if (!funcScope)
				throw std::runtime_error("No matching function: " + call->name);

			std::vector<int32_t> rawArgs;
			for (auto& v : argValues)
				rawArgs.push_back(v.raw_value);

			DebugValue res{};
			res.raw_value = vm.executeSandboxed(funcScope->start_pc, rawArgs);
			res.type_id = funcScope->type_id;
			return res;
		}

		case E_NEW:
		{
			auto newNode = static_cast<NewNode*>(node.get());
			const DebugScope* cls = debugData.resolveScope(newNode->class_name, currentScope);
			if (!cls)
				throw std::runtime_error("Unknown class: " + newNode->class_name);

			DebugValue res{};
			// TODO ! needs start_pc of the ctor.
			res.raw_value = vm.allocateObject(getClassIDFromScope(cls));
			res.type_id = getTypeIndexForClass(cls);
			return res;
		}

		case E_INDEX:
		{
			// TODO: implement.
		}
	}
	return {0, 0, false};
}

DebugValue ExpressionEvaluator::readValue(const DebugSymbol* sym)
{
	DebugValue v{};
	v.type_id = sym->type_id;
	v.is_lvalue = true;

	if (sym->storage == CONSTANT)
		v.raw_value = sym->offset;
	else if (sym->storage == LOC_GLOBAL)
		v.raw_value = vm.readGlobal(sym->offset);
	else if (sym->storage == LOC_STACK)
		v.raw_value = vm.readStack(sym->offset);
	else if (sym->storage == LOC_REGISTER)
		v.raw_value = vm.readRegister(sym->offset);
	else if (sym->storage == LOC_CLASS)
	{
		int32_t thisPtr = vm.getCurrentThisPointer();
		if (thisPtr == 0)
			throw std::runtime_error("'this' is null");
		v.raw_value = vm.readObjectMember(thisPtr, sym->offset);
	}
	return v;
}

DebugValue ExpressionEvaluator::evalBinaryOp(const std::string& op, DebugValue l, DebugValue r)
{
	int32_t l_val = l.raw_value;
	int32_t r_val = r.raw_value;
	const DebugType* lType = debugData.getTypeUnwrapConst(l.type_id);
	const DebugType* rType = debugData.getTypeUnwrapConst(r.type_id);
	bool resultIsFixed = l.isFixed() || r.isFixed();

	if (lType->tag == TYPE_BITFLAGS)
	{
		if (rType->tag != TYPE_BITFLAGS || lType->extra != rType->extra)
			throw std::runtime_error("Bitflags operation requires matching types.");

		if (op == "|" || op == "&" || op == "^")
		{
			DebugValue res = l;

			auto scope = &debugData.scopes[lType->extra];
			bool fixed = scope->type_id == TYPE_INT;
			int l_val = fixed ? l.raw_value / FIXED_ONE : l.raw_value;
			int r_val = fixed ? r.raw_value / FIXED_ONE : r.raw_value;

			if (op == "|") res.raw_value = l_val | r_val;
			if (op == "&") res.raw_value = l_val & r_val;
			if (op == "^") res.raw_value = l_val ^ r_val;

			if (fixed) res.raw_value *= FIXED_ONE;

			return res;
		}
		throw std::runtime_error("Invalid operation '" + op + "' for Bitflags.");
	}

	DebugValue res{};
	res.type_id = resultIsFixed ? TYPE_INT : TYPE_LONG;

	if (op == "+")
		res.raw_value = l_val + r_val;
	else if (op == "-")
		res.raw_value = l_val - r_val;
	else if (op == "*")
	{
		if (resultIsFixed)
		{
			int64_t bigRes = (int64_t)l_val * (int64_t)r_val;
			res.raw_value = (int32_t)(bigRes / FIXED_ONE);
		}
		else
			res.raw_value = l_val * r_val;
	}
	else if (op == "/")
	{
		if (r_val == 0)
			throw std::runtime_error("Divide by zero");
		if (resultIsFixed)
		{
			int64_t bigL = (int64_t)l_val * FIXED_ONE;
			res.raw_value = (int32_t)(bigL / r_val);
		}
		else
			res.raw_value = l_val / r_val;
	}
	return res;
}

const DebugScope* ExpressionEvaluator::getClassScope(int32_t typeIdx)
{
	if (typeIdx < DEBUG_TYPE_TAG_TABLE_START)
		return nullptr;
	const auto& dt = debugData.types[typeIdx - DEBUG_TYPE_TAG_TABLE_START];
	if (dt.tag == TYPE_CONST)
		return getClassScope(dt.extra);
	if (dt.tag == TYPE_CLASS)
		return &debugData.scopes[dt.extra];
	return nullptr;
}

const DebugScope* ExpressionEvaluator::resolveOverload(const std::string& name, const std::vector<int32_t>& argTypes, const DebugScope* scope)
{
	auto candidates = debugData.resolveFunctions(name, scope);

	const DebugScope* bestMatch = nullptr;
	int bestScore = -1;
	bool isAmbiguous = false;

	for (const auto* func : candidates)
	{
		std::vector<int32_t> paramTypes;
		auto paramSymbols = debugData.getChildSymbols(func);
		for (auto param : paramSymbols)
			paramTypes.push_back(param->type_id);

		if (paramTypes.size() != argTypes.size())
			continue;

		int score = 0;
		bool compatible = true;
		for (size_t i = 0; i < argTypes.size(); i++)
		{
			if (argTypes[i] == paramTypes[i]) {
				score += 10; // Exact match preference
			} else if (debugData.canCoerceTypes(argTypes[i], paramTypes[i])) {
				score += 1;  // Conversion penalty
			} else {
				compatible = false;
				break;
			}
		}

		if (compatible)
		{
			if (score > bestScore)
			{
				bestScore = score;
				bestMatch = func;
				isAmbiguous = false;
			}
			else if (score == bestScore)
			{
				isAmbiguous = true;
			}
		}
	}

	if (isAmbiguous)
		throw std::runtime_error("Ambiguous function call '" + name + "'. Multiple overloads match.");

	return bestMatch;
}

int32_t ExpressionEvaluator::getClassIDFromScope(const DebugScope* scope)
{
	// TODO ! del?
	return 0;
}

int32_t ExpressionEvaluator::getTypeIndexForClass(const DebugScope* scope)
{
	int scope_index = debugData.getScopeIndex(scope);

	for (int i = 0; i < debugData.types.size(); i++)
	{
		auto& type = debugData.types[i];
		if (type.tag == TYPE_CLASS && type.extra == scope_index)
			return i + DEBUG_TYPE_TAG_TABLE_START;
	}

	return 0;
}
