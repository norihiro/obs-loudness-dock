#pragma once
#include <QColor>

static inline QColor color_from_int(uint32_t val)
{
	return QColor(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff);
}
