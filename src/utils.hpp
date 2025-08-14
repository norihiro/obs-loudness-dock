#pragma once
#include <QColor>

static inline QColor color_from_int(uint32_t val)
{
	return QColor(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff);
}

static inline uint32_t color_int_from_text(const char *t)
{
	if (*t == '#')
		t++;
	uint32_t val = strtol(t, nullptr, 16);
	if (strlen(t) == 6)
		return ((val & 0xFF0000) >> 16) | (val & 0x00FF00) | ((val & 0x0000FF) << 16);
	return (((val & 0xF00) >> 8) * 0x000011) | (((val & 0x0F0) >> 4) * 0x001100) | ((val & 0x00F) * 0x110000);
}
