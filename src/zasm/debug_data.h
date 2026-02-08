#ifndef ZASM_DEBUG_DATA_H_
#define ZASM_DEBUG_DATA_H_

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/ints.h"
#include "zasm/pc.h"

struct SourceFile
{
	std::string path;
	// Empty if no ops come from this file (according to debug_lines_encoded).
	std::string contents;
};

struct DebugData
{
	static const int VERSION = 1;
	static const int DEBUG_LINE_OP_SIMPLE_STEP_MAX = 0xEF;
	static const int DEBUG_LINE_OP_SET_FILE = 0xF0;
	static const int DEBUG_LINE_OP_EXTENDED_STEP = 0xF1;
	static const int DEBUG_LINE_OP_PROLOGUE_END = 0xF2;

	static std::optional<DebugData> decode(const std::vector<byte>& buffer);

	std::vector<SourceFile> source_files;
	std::vector<byte> debug_lines_encoded;

	void appendLineInfoSetFile(int file);
	void appendLineInfoSimpleStep(byte d_pc);
	void appendLineInfoExtendedStep(int d_pc, int d_line);
	void appendLineInfoPrologueEnd();

	std::pair<const char*, int> resolveLocation(pc_t pc) const;

	std::vector<byte> encode() const;

private:
	struct Checkpoint
	{
		pc_t pc;
		size_t cursor; // Byte offset in debug_lines_encoded
		int32_t line;
		int32_t file_index;
	};

	struct CacheEntry
	{
		pc_t pc = -1; // -1 indicates empty
		std::pair<const char*, int> result;
	};

	mutable std::vector<Checkpoint> checkpoints;
	mutable bool checkpoints_built = false;
	void buildCheckpoints() const;

	mutable std::array<CacheEntry, 1024> resolve_location_cache;
};

#endif
