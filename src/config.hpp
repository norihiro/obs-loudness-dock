#pragma once
#include <vector>
#include <string>

struct loudness_dock_config_s
{
	struct tab_config
	{
		std::string name;
		int track = 0;
	};

	bool abbrev_label = false;

	std::vector<tab_config> tabs;

	std::vector<float> bar_thresholds;
	std::vector<uint32_t> bar_fg_colors;
	std::vector<uint32_t> bar_bg_colors;
};
