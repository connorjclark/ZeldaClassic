#include "base/frame_stream.h"
#include <cstring>
#include <zstd.h>

static constexpr char MAGIC[8] = {'Z', 'C', 'F', 'R', 'A', 'M', 'E', 'S'};
static constexpr uint32_t VERSION = 1;
// About 14 MB of a 256x232 screen before it's compressed. Larger chunks compress a bit better,
// but take longer to read a frame from.
static constexpr int FRAMES_PER_CHUNK = 240;
static constexpr int CHUNK_HEADER_SIZE = 20;
static constexpr int FRAME_HEADER_SIZE = 8;
static constexpr int ZSTD_LEVEL = 3;

static void put_u32(std::vector<uint8_t>& out, uint32_t value)
{
	for (int i = 0; i < 4; i++)
		out.push_back((value >> (i * 8)) & 0xFF);
}

static void put_u16(std::vector<uint8_t>& out, uint16_t value)
{
	out.push_back(value & 0xFF);
	out.push_back(value >> 8);
}

static uint32_t get_u32(const uint8_t* in)
{
	return in[0] | (in[1] << 8) | (in[2] << 16) | ((uint32_t)in[3] << 24);
}

static uint16_t get_u16(const uint8_t* in)
{
	return in[0] | (in[1] << 8);
}

FrameStreamWriter::~FrameStreamWriter()
{
	close();
}

bool FrameStreamWriter::open(const std::filesystem::path& path)
{
	close();
	file = fopen(path.string().c_str(), "wb");
	if (!file)
		return false;

	fwrite(MAGIC, 1, sizeof(MAGIC), file);
	std::vector<uint8_t> header;
	put_u32(header, VERSION);
	fwrite(header.data(), 1, header.size(), file);
	return true;
}

void FrameStreamWriter::add(int frame, int width, int height, const uint8_t* const* rows, const uint8_t* palette)
{
	if (!file)
		return;

	if (count == 0)
		first_frame = frame;
	last_frame = frame;

	std::vector<uint8_t> payload(768 + (size_t)width * height);
	memcpy(payload.data(), palette, 768);
	for (int y = 0; y < height; y++)
		memcpy(payload.data() + 768 + (size_t)y * width, rows[y], width);

	put_u32(raw, frame);
	put_u16(raw, width);
	put_u16(raw, height);
	size_t start = raw.size();
	raw.insert(raw.end(), payload.begin(), payload.end());
	// The first frame of a chunk, and one that changed size, are stored as they are.
	if (count > 0 && width == prev_width && height == prev_height)
	{
		for (size_t i = 0; i < payload.size(); i++)
			raw[start + i] ^= prev_payload[i];
	}

	prev_payload = std::move(payload);
	prev_width = width;
	prev_height = height;
	if (++count == FRAMES_PER_CHUNK)
		end_chunk();
	write_finished_chunks(false);
}

void FrameStreamWriter::end_chunk()
{
	if (count == 0)
		return;

	auto compress = [raw = std::move(raw), count = count, first_frame = first_frame, last_frame = last_frame]() {
		Chunk chunk;
		chunk.data.resize(ZSTD_compressBound(raw.size()));
		size_t size = ZSTD_compress(chunk.data.data(), chunk.data.size(), raw.data(), raw.size(), ZSTD_LEVEL);
		if (ZSTD_isError(size))
		{
			chunk.data.clear();
			return chunk;
		}
		chunk.data.resize(size);
		put_u32(chunk.header, size);
		put_u32(chunk.header, raw.size());
		put_u32(chunk.header, first_frame);
		put_u32(chunk.header, last_frame);
		put_u32(chunk.header, count);
		return chunk;
	};
#ifdef __EMSCRIPTEN__
	// Done when it's written instead, as the web build may not have threads to spare.
	pending.push_back(std::async(std::launch::deferred, std::move(compress)));
#else
	pending.push_back(std::async(std::launch::async, std::move(compress)));
#endif

	raw = {};
	prev_payload.clear();
	count = 0;
}

void FrameStreamWriter::write_finished_chunks(bool wait)
{
	while (!pending.empty())
	{
		auto& next = pending.front();
		if (!wait && next.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			break;

		Chunk chunk = next.get();
		pending.pop_front();
		if (file && !chunk.data.empty())
		{
			fwrite(chunk.header.data(), 1, chunk.header.size(), file);
			fwrite(chunk.data.data(), 1, chunk.data.size(), file);
		}
	}
}

void FrameStreamWriter::flush()
{
	if (!file)
		return;

	// The frames added after this start a chunk of their own.
	end_chunk();
	write_finished_chunks(true);
	fflush(file);
}

void FrameStreamWriter::close()
{
	if (!file)
		return;

	flush();
	fclose(file);
	file = nullptr;
}

std::optional<IndexedFrame> read_frame_stream(const std::filesystem::path& path, int frame)
{
	FILE* file = fopen(path.string().c_str(), "rb");
	if (!file)
		return std::nullopt;

	std::optional<IndexedFrame> result;
	uint8_t header[CHUNK_HEADER_SIZE];
	char magic[sizeof(MAGIC)];
	if (fread(magic, 1, sizeof(magic), file) != sizeof(magic) || memcmp(magic, MAGIC, sizeof(MAGIC)) ||
		fread(header, 1, 4, file) != 4 || get_u32(header) != VERSION)
	{
		fclose(file);
		return std::nullopt;
	}

	// The last chunk that starts at or before the frame has it. A chunk cut short, like by a
	// crash, ends the file.
	long chunk_offset = -1;
	uint32_t chunk_size = 0, chunk_raw_size = 0;
	while (fread(header, 1, CHUNK_HEADER_SIZE, file) == CHUNK_HEADER_SIZE)
	{
		uint32_t size = get_u32(header);
		int first = (int32_t)get_u32(header + 8);
		long offset = ftell(file);
		if (fseek(file, size, SEEK_CUR) || first > frame)
			break;
		chunk_offset = offset;
		chunk_size = size;
		chunk_raw_size = get_u32(header + 4);
	}

	std::vector<uint8_t> data(chunk_size), raw(chunk_raw_size);
	bool read = chunk_offset != -1 && !fseek(file, chunk_offset, SEEK_SET) &&
		fread(data.data(), 1, chunk_size, file) == chunk_size;
	fclose(file);
	if (!read || ZSTD_decompress(raw.data(), raw.size(), data.data(), data.size()) != raw.size())
		return std::nullopt;

	std::vector<uint8_t> payload;
	int prev_width = 0, prev_height = 0;
	size_t pos = 0;
	while (pos + FRAME_HEADER_SIZE <= raw.size())
	{
		int this_frame = (int32_t)get_u32(&raw[pos]);
		int width = get_u16(&raw[pos + 4]);
		int height = get_u16(&raw[pos + 6]);
		size_t size = 768 + (size_t)width * height;
		pos += FRAME_HEADER_SIZE;
		if (this_frame > frame || pos + size > raw.size())
			break;

		bool is_delta = !payload.empty() && width == prev_width && height == prev_height;
		if (is_delta)
		{
			for (size_t i = 0; i < size; i++)
				payload[i] ^= raw[pos + i];
		}
		else
			payload.assign(raw.begin() + pos, raw.begin() + pos + size);
		pos += size;
		prev_width = width;
		prev_height = height;

		result.emplace();
		result->frame = this_frame;
		result->width = width;
		result->height = height;
	}

	if (result)
	{
		memcpy(result->palette.data(), payload.data(), 768);
		result->pixels.assign(payload.begin() + 768, payload.end());
	}
	return result;
}
