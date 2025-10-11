#pragma once
#include <vector>
#include <string>

struct loudness_dock_config_s
{
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

	std::vector<tab_config> tabs;

	std::vector<float> bar_thresholds;
	std::vector<uint32_t> bar_fg_colors;
	std::vector<uint32_t> bar_bg_colors;
};
