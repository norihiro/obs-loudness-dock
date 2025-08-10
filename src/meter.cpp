/*
OBS Loudness Dock
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
#include "meter.hpp"

#include "plugin-macros.generated.h"

struct color_s {
	double level;
	QColor color_fg;
	QColor color_bg;
};

struct private_data {
	double min = -50.0;
	double max = 0.0;
	double current = -99;

	int tick_height;
	int font_height, font_width;

	QFont font;
	std::vector<color_s> colors;

	inline int toX(double value, const QRect &widgetRect) const
	{
		if (value <= min)
			value = min;
		if (value >= max)
			value = max;
		double x = (value - min) / (max - min);
		return (int)((widgetRect.width() - font_width) * x) + font_width / 2;
	}
};

SingleMeter::SingleMeter(QWidget *parent)
	: QWidget(parent),
	data(*new struct private_data)
{
	data.colors.push_back({-23.0, QColor(0, 0, 255), QColor(0, 0, 85)});
	data.colors.push_back({-14.0, QColor(0, 255, 0), QColor(0, 85, 0)});
	data.colors.push_back({  0.0, QColor(255, 0, 0), QColor(85, 0, 0)});

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
	delete &data;
}

void SingleMeter::setRange(double min, double max)
{
	data.min = min;
	data.max = max;
}

void SingleMeter::setColors(const double *levels, const uint32_t *colors, int size)
{
}

void SingleMeter::setLevel(double level)
{
	data.current = level;

	update();
}

void SingleMeter::paintEvent(QPaintEvent *event)
{
	QRect widgetRect = rect();
	int width = widgetRect.width();
	int height = widgetRect.height();

	QPainter painter(this);

	double last = data.min;
	for (int ix = 0; ix < data.colors.size(); ix++) {
		const auto &c = data.colors[ix];

		QRect fill_rect;
		fill_rect.setTop(0);
		fill_rect.setBottom(height - data.tick_height - data.font_height);

		if (last < data.current) {
			fill_rect.setLeft(data.toX(last, widgetRect));
			fill_rect.setRight(data.toX(std::min(data.current, c.level), widgetRect));
			painter.fillRect(fill_rect, c.color_fg);
		}

		if (data.current < c.level) {
			fill_rect.setLeft(data.toX(std::max(data.current, last), widgetRect));
			fill_rect.setRight(data.toX(c.level, widgetRect));
			painter.fillRect(fill_rect, c.color_bg);
		}

		last = c.level;
	}

	painter.setFont(data.font);
	QFontMetrics metrics(data.font);

	double tick_step = (data.max - data.min) * data.font_width / width * 1.2;
	tick_step = std::ceil(tick_step / 10.0) * 10.0;
	double tick_start = std::ceil(data.min / tick_step) * tick_step;
	for (double tick = tick_start; tick <= data.max; tick += tick_step) {
		int pos = data.toX(tick, widgetRect);
		int y2 = height - data.font_height;
		int y1 = y2 - data.tick_height;

		painter.drawLine(pos, y1, pos, y2);

		QString str = QString::number(tick);
		QRect b = metrics.boundingRect(str);
		painter.drawText(pos - b.width() / 2, height, str);
	}
}
