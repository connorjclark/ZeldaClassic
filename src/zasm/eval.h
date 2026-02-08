#ifndef ZASM_EVAL_H_
#define ZASM_EVAL_H_

#include "zasm/debug_data.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

struct DebugValue
{
	int32_t raw_value;    // The bits in memory
	uint32_t type_id;
	bool is_lvalue;       // Can we assign to this?

	bool isFixed() const { return type_id == TYPE_INT; }
	bool isLong() const { return type_id == TYPE_LONG;}
	bool isBool() const { return type_id == TYPE_BOOL;}
};

// Math Constants
constexpr int32_t FIXED_ONE = 10000;

enum ExprType {
	E_LITERAL,
	E_VAR,
	E_BINARY,
	E_UNARY,
	E_CALL,
	E_NEW,
	E_MEMBER,
	E_INDEX
};

struct ExprNode {
	ExprType type;
	virtual ~ExprNode() = default;
};

struct LiteralNode : ExprNode {
	DebugValue value;
	LiteralNode(DebugValue v) : value(v) { type = E_LITERAL; }
};

struct VarNode : ExprNode {
	std::string identifier;
	VarNode(std::string id) : identifier(id) { type = E_VAR; }
};

struct BinaryOpNode : ExprNode {
	std::string op; // "+", "-", "*", "/"
	std::shared_ptr<ExprNode> left;
	std::shared_ptr<ExprNode> right;
	BinaryOpNode(std::string o, std::shared_ptr<ExprNode> l, std::shared_ptr<ExprNode> r) 
		: op(o), left(l), right(r) { type = E_BINARY; }
};

struct UnaryOpNode : ExprNode {
	std::string op;
	std::shared_ptr<ExprNode> operand;
	UnaryOpNode(std::string o, std::shared_ptr<ExprNode> r) 
		: op(o), operand(r) { type = E_UNARY; }
};

struct MemberAccessNode : ExprNode {
	std::shared_ptr<ExprNode> object;
	std::string member_name;
	MemberAccessNode(std::shared_ptr<ExprNode> obj, std::string name) 
		: object(obj), member_name(name) { type = E_MEMBER; }
};

struct FuncCallNode : ExprNode {
	std::string name;
	std::vector<std::shared_ptr<ExprNode>> args;
	std::shared_ptr<ExprNode> object_context; // Null if global function, set if obj->method()
	
	FuncCallNode(std::string n, std::vector<std::shared_ptr<ExprNode>> a) 
		: name(n), args(a), object_context(nullptr) { type = E_CALL; }
};

struct NewNode : ExprNode {
	std::string class_name;
	NewNode(std::string cn) : class_name(cn) { type = E_NEW; }
};

struct IndexNode : ExprNode {
	std::shared_ptr<ExprNode> base;
	std::shared_ptr<ExprNode> index;

	IndexNode(std::shared_ptr<ExprNode> b, std::shared_ptr<ExprNode> i)
		: base(b), index(i) { type = E_INDEX; }
};

class ExpressionParser
{
	std::string input;
	size_t pos = 0;

	char peek();
	char get();
	void skipWhitespace();
	bool match(char c);

	std::shared_ptr<ExprNode> parseLogicalOr();
	std::shared_ptr<ExprNode> parseLogicalAnd();
	std::shared_ptr<ExprNode> parseBitOr();
	std::shared_ptr<ExprNode> parseBitXor();
	std::shared_ptr<ExprNode> parseBitAnd();
	std::shared_ptr<ExprNode> parseEquality();
	std::shared_ptr<ExprNode> parseRelational();
	std::shared_ptr<ExprNode> parseAdd();
	std::shared_ptr<ExprNode> parseFactor();
	std::shared_ptr<ExprNode> parseUnary();
	std::shared_ptr<ExprNode> parsePrimary();

public:
	ExpressionParser(std::string s);
	std::shared_ptr<ExprNode> parseExpression();
};

class VMInterface {
public:
    virtual ~VMInterface() = default;

    // Core Memory Access
    virtual int32_t readStack(int32_t offset_from_fp) = 0; // fp = Frame Pointer
    virtual int32_t readGlobal(int32_t global_index) = 0;
    virtual int32_t readRegister(int32_t reg_id) = 0;
    virtual int32_t readObjectMember(int32_t object_ptr, int32_t member_offset) = 0;

    // Execution Control
    // Runs code starting at 'pc' until it hits a 'RET' opcode or error
    virtual int32_t executeSandboxed(pc_t start_pc, const std::vector<int32_t>& args) = 0;
    
    // Memory Management
    virtual int32_t allocateObject(int32_t class_idx) = 0;

    // Helper: Find 'this' pointer for the current frame
    virtual int32_t getCurrentThisPointer() = 0;
};

class ExpressionEvaluator
{
	const DebugData& debugData;
	const DebugScope* currentScope;
	VMInterface& vm;

	DebugValue readValue(const DebugSymbol* sym);
	DebugValue evalBinaryOp(const std::string& op, DebugValue l, DebugValue r);
	const DebugScope* getClassScope(int32_t typeIdx);

	const DebugScope* resolveOverload(const std::string& name, const std::vector<int32_t>& args, const DebugScope* scope);
	int32_t getClassIDFromScope(const DebugScope* scope);
	int32_t getTypeIndexForClass(const DebugScope* scope);

public:
	ExpressionEvaluator(const DebugData& dd, const DebugScope* scope, VMInterface& v);
	DebugValue evaluate(std::shared_ptr<ExprNode> node);
};

#endif
