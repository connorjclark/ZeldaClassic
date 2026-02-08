#include "zasm/debug_data.h"
#include "base/ints.h"
#include "zasm/pc.h"

#include <array>
#include <cstdint>
#include <fmt/ranges.h>
#include <optional>
#include <string>
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

std::optional<DebugData> DebugData::decode(const std::vector<byte>& buffer)
{
	if (buffer.empty())
		return std::nullopt;

	DebugData result;
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
	if (buffer.size() - cursor != debug_lines_size)
		return std::nullopt;
	result.debug_lines_encoded.assign(buffer.begin() + cursor, buffer.end());

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
