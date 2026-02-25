#pragma once
#include <vector>
#include <string>

struct loudness_dock_config_s
{
	enum timescale_flags_e : uint32_t {
		timescale_momentary = 1 << 0,
		timescale_shortterm = 1 << 1,
		timescale_integrated = 1 << 2,
		timescale_range = 1 << 3,
		timescale_peak = 1 << 4,
	};

	enum trigger_mode_e {
		trigger_none = 0,
		trigger_streaming = 1,
		trigger_recording = 2,
		trigger_both = 3,
	};

	struct tab_config
	{
		std::string name;
		int track = 0;
		trigger_mode_e trigger_mode = trigger_none;
	};

	bool abbrev_label = false;
	uint32_t hide_timescales = 0;

	std::vector<tab_config> tabs;

	std::vector<float> bar_thresholds;
	std::vector<uint32_t> bar_fg_colors;
	std::vector<uint32_t> bar_bg_colors;
};
