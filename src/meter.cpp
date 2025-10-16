/*
Loudness Dock for OBS Studio
Copyright (C) 2025 Norihiro Kamae <norihiro@nagater.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <cmath>
#include <obs-module.h>
#include <obs.h>
#include <vector>
#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#ifdef ENABLE_PROFILE
#include <util/profiler.hpp>
#endif
#include "meter.hpp"
#include "utils.hpp"

#include "plugin-macros.generated.h"

struct color_s
{
	float level;
	QColor color_fg;
	QColor color_bg;
};

struct private_data
{
	float min = -59.0f;
	float max = -5.0f;
	float current = -99;

	int current_int = -1;

	int tick_height;
	int font_height, font_width;

	QFont font;
	std::vector<color_s> colors;

	inline int toX(float value, const QRect &widgetRect) const
	{
		if (value <= min)
			value = min;
		if (value >= max)
			value = max;
		float x = (value - min) / (max - min);
		return (int)((widgetRect.width() - font_width) * x) + font_width / 2;
	}
};

SingleMeter::SingleMeter(QWidget *parent) : QWidget(parent), data(*new struct private_data)
{
	ASSERT_THREAD(OBS_TASK_UI);

	data.colors.push_back({-23.0, QColor(0, 0, 255), QColor(0, 0, 85)});
	data.colors.push_back({-14.0, QColor(0, 255, 0), QColor(0, 85, 0)});
	data.colors.push_back({0.0, QColor(255, 0, 0), QColor(85, 0, 0)});

	data.font = font();
	QFontInfo info(data.font);
	data.font.setPointSizeF(info.pointSizeF() * 0.7);
	data.font.setFixedPitch(true);
	QFontMetrics metrics(data.font);
	setMinimumSize(64, 8 + metrics.capHeight());
	QRect scaleBounds = metrics.boundingRect("-88");

	data.font_height = metrics.capHeight();
	data.font_width = scaleBounds.width();

	data.tick_height = 4;
}

SingleMeter::~SingleMeter()
{
	ASSERT_THREAD(OBS_TASK_UI);

	delete &data;
}

void SingleMeter::setRange(float min, float max)
{
	ASSERT_THREAD(OBS_TASK_UI);

	data.min = min;
	data.max = max;

	update();
}

void SingleMeter::setColors(const float *levels, const uint32_t *fg_colors, const uint32_t *bg_colors,
			    uint32_t n_colors)
{
	ASSERT_THREAD(OBS_TASK_UI);

	data.colors.resize(n_colors);

	for (uint32_t i = 0; i < n_colors; i++) {
		data.colors[i].level = i < n_colors - 1 ? levels[i] : data.max;
		data.colors[i].color_fg = color_from_int(fg_colors[i]);
		data.colors[i].color_bg = color_from_int(bg_colors[i]);
	}

	update();
}

void SingleMeter::setLevel(float level)
{
	ASSERT_THREAD(OBS_TASK_UI);

	data.current = level;

	/* Update only the bar. */
	QRect widgetRect = rect();

	int next_int = data.toX(level, widgetRect);
	if (next_int == data.current_int)
		return;

	int x0 = std::max(std::min(next_int, data.current_int) - 1, 0);
	int x1 = std::min(std::max(next_int, data.current_int) + 1, widgetRect.width());
	QRect rect(x0, 0, x1 - x0, widgetRect.height() - data.tick_height - data.font_height + 1);
	update(rect);
}

#ifdef ENABLE_PROFILE
static const char *name_paintEvent = "SingleMeter::paintEvent";
#endif

void SingleMeter::paintEvent(QPaintEvent *event)
{
#ifdef ENABLE_PROFILE
	ScopeProfiler profiler(name_paintEvent);
#endif
	ASSERT_THREAD(OBS_TASK_UI);

	QRect widgetRect = rect();
	int width = widgetRect.width();
	int height = widgetRect.height();

	QPainter painter(this);

	data.current_int = data.toX(data.current, widgetRect);

	int last = data.toX(data.min, widgetRect);
	for (uint32_t ix = 0; ix < data.colors.size(); ix++) {
		const auto &c = data.colors[ix];
		int level_int = data.toX(c.level, widgetRect);

		QRect fill_rect;
		fill_rect.setTop(0);
		fill_rect.setBottom(height - data.tick_height - data.font_height);

		if (last < data.current_int) {
			fill_rect.setLeft(last);
			fill_rect.setRight(std::min(data.current_int, level_int));
			painter.fillRect(fill_rect, c.color_fg);
		}

		if (data.current_int < level_int) {
			fill_rect.setLeft(std::max(data.current_int, last));
			fill_rect.setRight(level_int);
			painter.fillRect(fill_rect, c.color_bg);
		}

		last = level_int;
	}

	const int y2 = height - data.font_height;
	const int y1 = y2 - data.tick_height;

	/* Skip drawing ticks and labels */
	if (event->region().boundingRect().bottom() <= y1)
		return;

	painter.setFont(data.font);
	QFontMetrics metrics(data.font);

	float tick_step = (data.max - data.min) * data.font_width / width * 1.2;
	tick_step = std::ceil(tick_step / 10.0) * 10.0;
	float tick_start = std::ceil(data.min / tick_step) * tick_step;
	for (float tick = tick_start; tick <= data.max; tick += tick_step) {
		int pos = data.toX(tick, widgetRect);

		painter.drawLine(pos, y1, pos, y2);

		QString str = QString::number(tick);
		QRect b = metrics.boundingRect(str);
		painter.drawText(pos - b.width() / 2, height, str);
	}
}
