#pragma once
#include <vector>

struct loudness_dock_config_s
{
	bool abbrev_label = false;
	std::vector<float> bar_thresholds;
	std::vector<uint32_t> bar_fg_colors;
	std::vector<uint32_t> bar_bg_colors;
};
