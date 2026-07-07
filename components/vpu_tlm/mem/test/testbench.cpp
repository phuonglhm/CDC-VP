// Simple unit tests for FrameMemory
#include <iostream>
#include <cstdint>
#include "frame_memory.h"

using namespace cdc::components;

static int failures = 0;
static void check_eq(const char* name, uint32_t exp, uint32_t got) {
	if (exp != got) {
		std::cerr << "FAIL: " << name << " expected=" << exp << " got=" << got << "\n";
		++failures;
	} else {
		std::cout << "PASS: " << name << "\n";
	}
}

int sc_main(int argc, char* argv[]) {
	FrameMemory mem("frame_mem");

	// Prepare a small 16x16 frame and set a few unique pixels
	frame f(16, 16);
	f.fill(10, 128, 128);
	f.set_luma(0, 0, 7);
	f.set_luma(1, 0, 8);
	f.set_luma(0, 1, 9);
	f.set_cb(0, 0, 50);
	f.set_cr(0, 0, 60);

	// Load frame and verify active frame copy
	mem.load_frame(1, f);
	frame got;
	if (!mem.get_active_frame(got)) {
		std::cerr << "FAIL: get_active_frame returned false\n";
		return 1;
	}
	check_eq("active_luma_0_0", 7u, got.get_luma(0,0));
	check_eq("active_cb_0_0", 50u, got.get_cb(0,0));
	check_eq("active_cr_0_0", 60u, got.get_cr(0,0));

	// Test getRefBlock on luma plane (size4x4==0 -> N=4, ext=6)
	RefBlock out;
	uint8_t size4x4 = 0;
	bool ok = mem.getRefBlock(RecPlane::Y, 0, 0, size4x4, PaddingMode::EDGE, out);
	if (!ok) { std::cerr << "FAIL: getRefBlock returned false\n"; ++failures; }
	uint32_t ext = static_cast<uint32_t>(recSizeToPixels(size4x4) + 2);
	// central top-left (r=1,c=1) corresponds to frame pixel (1,1)
	if (out.data.size() >= (size_t)ext * ext) {
		uint8_t center = out.data[1 * ext + 1];
		check_eq("getRefBlock_center_equals_frame_1_1", f.get_luma(1,1), center);
	} else {
		std::cerr << "FAIL: getRefBlock returned insufficient data\n";
		++failures;
	}

	// Test pushRefBlock writes central N x N into the frame
	RefBlock blk;
	blk.width = ext; blk.height = ext; blk.stride = ext;
	blk.data.assign(ext * ext, 0);
	// set central N x N region to 200
	const uint32_t N = recSizeToPixels(size4x4);
	for (uint32_t r = 1; r <= N; ++r) {
		for (uint32_t c = 1; c <= N; ++c) {
			blk.data[r * ext + c] = 200;
		}
	}

	mem.pushRefBlock(RecPlane::Y, 0, 0, size4x4, blk, 42);
	frame after;
	mem.get_active_frame(after);
	check_eq("pushRefBlock_wrote_pixel", 200u, after.get_luma(1,1));

	uint64_t ver = mem.regionVersion(RecPlane::Y, 0, 0, size4x4);
	if (ver != 42) { std::cerr << "FAIL: regionVersion expected 42 got " << ver << "\n"; ++failures; }
	else std::cout << "PASS: regionVersion\n";

	// Zero padding: request far outside frame and expect zeros
	RefBlock zblk;
	bool zok = mem.getRefBlock(RecPlane::Y, 1000, 1000, size4x4, PaddingMode::ZERO, zblk);
	if (!zok) { std::cerr << "FAIL: getRefBlock zero mode returned false\n"; ++failures; }
	for (auto v : zblk.data) { if (v != 0) { std::cerr << "FAIL: zero padding non-zero\n"; ++failures; break; } }

	if (failures) {
		std::cerr << failures << " test(s) failed\n";
		return 1;
	}

	std::cout << "All tests passed\n";
	return 0;
}
