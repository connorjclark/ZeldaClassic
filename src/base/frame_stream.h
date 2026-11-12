#ifndef BASE_FRAME_STREAM_H_
#define BASE_FRAME_STREAM_H_

#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <future>
#include <optional>
#include <vector>

// A file of 8-bit indexed pictures of a game's frames, each with its palette, compressed together
// so that every frame of a replay takes little space. Test replays keep one of each frame that
// looks different from the last, so the editor can show how a frame looked when the replay was
// recorded.
//
// The file is a header, then chunks of frames, each compressed on its own with zstd so that a
// frame can be read without decompressing the whole file. Within a chunk each frame is stored
// XOR'd with the one before it, since most of a frame is usually the same as the last.

struct IndexedFrame
{
	int frame = -1;
	int width = 0;
	int height = 0;
	// 256 RGB entries, each 0-255.
	std::array<uint8_t, 768> palette{};
	// width * height palette indices, row by row.
	std::vector<uint8_t> pixels;
};

class FrameStreamWriter
{
public:
	~FrameStreamWriter();

	// Starts a new file at the path, replacing what's there.
	bool open(const std::filesystem::path& path);
	bool is_open() const { return file != nullptr; }
	// Adds a frame, from its rows of palette indices. Frames are added in order.
	void add(int frame, int width, int height, const uint8_t* const* rows, const uint8_t* palette);
	// Writes all the frames added so far to the file.
	void flush();
	void close();

private:
	struct Chunk
	{
		std::vector<uint8_t> header;
		std::vector<uint8_t> data;
	};

	FILE* file = nullptr;
	// The frames of the chunk being added to, before they're compressed.
	std::vector<uint8_t> raw;
	// The palette and pixels of the last frame added to it, which the next is XOR'd with.
	std::vector<uint8_t> prev_payload;
	int prev_width = 0;
	int prev_height = 0;
	int count = 0;
	int first_frame = 0;
	int last_frame = 0;
	// Chunks being compressed off the main thread, so a recording doesn't stall, in the order
	// they go in the file.
	std::deque<std::future<Chunk>> pending;

	void end_chunk();
	void write_finished_chunks(bool wait);
};

// Reads the picture shown at a frame: the last one saved at or before it.
std::optional<IndexedFrame> read_frame_stream(const std::filesystem::path& path, int frame);

#endif
