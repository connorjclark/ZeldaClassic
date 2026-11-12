#include "base/frame_stream.h"
#include "test_runner/test_runner.h"
#include "test_runner/assert.h"
#include <filesystem>

static IndexedFrame make_frame(int frame, int width, int height)
{
	IndexedFrame f;
	f.frame = frame;
	f.width = width;
	f.height = height;
	for (int i = 0; i < 768; i++)
		f.palette[i] = (i + frame) % 64;
	f.pixels.resize(width * height);
	// Mostly the same from frame to frame, like a game's screen.
	for (int i = 0; i < width * height; i++)
		f.pixels[i] = (i % 7 == 0) ? (uint8_t)(i + frame) : (uint8_t)(i / width);
	return f;
}

static void add_frame(FrameStreamWriter& writer, const IndexedFrame& f)
{
	std::vector<const uint8_t*> rows;
	for (int y = 0; y < f.height; y++)
		rows.push_back(f.pixels.data() + y * f.width);
	writer.add(f.frame, f.width, f.height, rows.data(), f.palette.data());
}

static void assert_same(const std::optional<IndexedFrame>& got, const IndexedFrame& expected)
{
	assertTrue(got.has_value());
	assertEqual(got->frame, expected.frame);
	assertEqual(got->width, expected.width);
	assertEqual(got->height, expected.height);
	assertTrue(got->palette == expected.palette);
	assertTrue(got->pixels == expected.pixels);
}

static void test_round_trip()
{
	auto path = std::filesystem::temp_directory_path() / "zc_frame_stream_test.frames";
	std::vector<IndexedFrame> frames;
	// Enough for a few chunks, with gaps between frames, a change of size, and a flush partway
	// (as saving a replay partway through a recording does).
	for (int i = 0; i < 700; i++)
		frames.push_back(make_frame(10 + i * 2, i < 400 ? 32 : 40, i < 400 ? 24 : 30));

	{
		FrameStreamWriter writer;
		assertTrue(writer.open(path));
		for (int i = 0; i < frames.size(); i++)
		{
			add_frame(writer, frames[i]);
			if (i == 300)
				writer.flush();
		}
	}

	assertTrue(!read_frame_stream(path, 9).has_value());
	assert_same(read_frame_stream(path, 10), frames[0]);
	// Between two saved frames, it's the earlier one that was shown.
	assert_same(read_frame_stream(path, 11), frames[0]);
	for (int i : {1, 239, 240, 241, 300, 301, 399, 400, 401, 699})
		assert_same(read_frame_stream(path, frames[i].frame), frames[i]);
	assert_same(read_frame_stream(path, 100000), frames.back());

	std::filesystem::remove(path);
	assertTrue(!read_frame_stream(path, 10).has_value());
}

// Like playing the pictures back: every frame in order, then over again from the start.
static void test_play_back()
{
	auto path = std::filesystem::temp_directory_path() / "zc_frame_stream_play_test.frames";
	std::vector<IndexedFrame> frames;
	for (int i = 0; i < 600; i++)
		frames.push_back(make_frame(5 + i * 3, i < 450 ? 32 : 16, i < 450 ? 24 : 12));
	{
		FrameStreamWriter writer;
		assertTrue(writer.open(path));
		for (auto& f : frames)
			add_frame(writer, f);
	}

	FrameStreamReader reader;
	assertTrue(reader.open(path));
	assertEqual(reader.last_frame(), frames.back().frame);
	for (int pass = 0; pass < 2; pass++)
	{
		assertTrue(reader.at(4) == nullptr);
		size_t next = 0;
		for (int frame = 5; frame <= reader.last_frame() + 10; frame++)
		{
			while (next + 1 < frames.size() && frames[next + 1].frame <= frame)
				next++;
			const IndexedFrame* got = reader.at(frame);
			assert_same(got ? std::optional(*got) : std::nullopt, frames[next]);
		}
	}
	// Back within the same chunk.
	assert_same(*reader.at(frames[590].frame), frames[590]);
	assert_same(*reader.at(frames[500].frame), frames[500]);

	std::filesystem::remove(path);
}

TestResults test_frame_stream(bool verbose)
{
	TestResults tr{};
	struct { const char* name; void (*fn)(); } tests[] = {
		{ "round_trip", test_round_trip },
		{ "play_back", test_play_back },
	};

	for (auto& test : tests)
	{
		++tr.total;
		try
		{
			test.fn();
			if (verbose)
				fmt::println("  [PASS] {}", test.name);
		}
		catch (const std::exception& e)
		{
			++tr.failed;
			fmt::println("  [FAIL] {}\n{}", test.name, e.what());
		}
	}

	return tr;
}
