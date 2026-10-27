// Host test for rpcs3/Emu/Cell/Modules/etk_audio_conceal.h (GTK_AUDIO_CONCEAL, 0.10.2).
// Build + run:  g++ -std=c++17 -O1 -Wall -Wextra -I <armsx3-tree>/rpcs3/Emu/Cell/Modules tests/test_audio_conceal.cpp -o /tmp/t && /tmp/t
// Pins: a run is sample-continuous at every internal junction (ping-pong), the first
// concealed block starts on the last real sample, the last block under the cap fades to
// zero, past the cap is silence, the resume ramp lands the first real frame on the tail and
// dies out, and no block ever exceeds the source's amplitude.
#include "etk_audio_conceal.h"
#include <cmath>
#include <cstdio>
#include <vector>

static int fails = 0;
#define CHECK(name, cond) do { if (cond) std::printf("    ok   %s\n", name); else { std::printf("    FAIL %s\n", name); fails++; } } while (0)

int main()
{
	const uint32_t F = 256, CH = 2, MAX = 8;
	// source: a 1 kHz-ish tone, stereo with a channel offset, peak 0.8
	std::vector<float> src(F * CH);
	for (uint32_t f = 0; f < F; f++) { src[f*CH] = 0.8f * std::sin(0.13f * f); src[f*CH+1] = 0.8f * std::cos(0.13f * f); }
	float tail[CH] = { src[(F-1)*CH], src[(F-1)*CH+1] };

	std::vector<std::vector<float>> blocks;
	for (uint32_t run = 0; run < MAX + 2; run++)
	{
		std::vector<float> dst(F * CH, 123.f);
		const bool sig = etk_audio_conceal::fill(dst.data(), src.data(), F, CH, run, MAX);
		CHECK(run < MAX ? "block under the cap carries signal" : "block past the cap reports silence", sig == (run < MAX));
		blocks.push_back(dst);
	}

	// first concealed block starts on the last real sample (reversed copy)
	CHECK("first concealed frame == last real frame (both channels)",
	      blocks[0][0] == tail[0] && blocks[0][1] == tail[1]);
	// internal junctions: block k's last frame == block k+1's first frame, for k+1 < MAX-1 (before the fade)
	bool cont = true;
	for (uint32_t k = 0; k + 1 < MAX - 1; k++)
		for (uint32_t c = 0; c < CH; c++)
			if (std::fabs(blocks[k][(F-1)*CH + c] - blocks[k+1][c]) > 1e-6f) cont = false;
	CHECK("ping-pong: every junction inside the run is sample-continuous", cont);
	// faded block: starts near its unfaded value, ends at exactly zero
	const auto& last = blocks[MAX-1];
	CHECK("last block under the cap ends at zero", last[(F-1)*CH] == 0.f && last[(F-1)*CH+1] == 0.f);
	CHECK("last block under the cap starts with (nearly) full gain", std::fabs(last[0] - blocks[MAX-3][0]) < 0.01f);
	// past the cap: all zeros
	bool zeros = true; for (float v : blocks[MAX]) if (v != 0.f) zeros = false; for (float v : blocks[MAX+1]) if (v != 0.f) zeros = false;
	CHECK("blocks past the cap are silence", zeros);
	// amplitude never exceeds the source
	float peak = 0; for (auto& b : blocks) for (float v : b) peak = std::max(peak, std::fabs(v));
	CHECK("no concealed block exceeds the source's peak", peak <= 0.8f + 1e-6f);

	// resume ramp: real block starting at +0.5 after a tail of -0.3 on ch0; ch1 tail equals the block (no-op)
	std::vector<float> real(F * CH);
	for (uint32_t f = 0; f < F; f++) { real[f*CH] = 0.5f; real[f*CH+1] = 0.1f; }
	const float t2[CH] = { -0.3f, 0.1f };
	std::vector<float> ramped = real;
	etk_audio_conceal::ramp_resume(ramped.data(), t2, F, CH);
	const uint32_t L = etk_audio_conceal::RESUME_RAMP_FRAMES;
	CHECK("resume: first frame lands within one ramp step of the tail", std::fabs(ramped[0] - (-0.3f)) <= 0.8f / L + 1e-6f);
	bool mono = true; for (uint32_t f = 1; f < L; f++) if (ramped[f*CH] < ramped[(f-1)*CH]) mono = false;
	CHECK("resume: the correction decays monotonically", mono);
	CHECK("resume: frame L and beyond are untouched", ramped[L*CH] == 0.5f && ramped[(F-1)*CH] == 0.5f);
	bool ch1same = true; for (uint32_t f = 0; f < F; f++) if (ramped[f*CH+1] != 0.1f) ch1same = false;
	CHECK("resume: a channel already on its tail is untouched", ch1same);

	// degenerate geometry does not write out of bounds / divides by zero
	std::vector<float> d0(4, 1.f);
	CHECK("fill with frames=0 is silence", etk_audio_conceal::fill(d0.data(), src.data(), 0, CH, 0, MAX) == false);
	etk_audio_conceal::ramp_resume(d0.data(), t2, 0, CH);

	std::printf("%s (%d fail)\n", fails ? "FAIL" : "PASS", fails);
	return fails ? 1 : 0;
}
