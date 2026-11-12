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

TestResults test_frame_stream(bool verbose)
{
	TestResults tr{};
	struct { const char* name; void (*fn)(); } tests[] = {
		{ "round_trip", test_round_trip },
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
