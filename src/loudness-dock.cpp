/*
OBS Main View Source Plugin
Copyright (C) 2023 Norihiro Kamae <norihiro@nagater.net>

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

#include <obs-module.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include "plugin-macros.generated.h"
#include "loudness-dock.hpp"

LoudnessDock::LoudnessDock(QWidget *parent) : QFrame(parent)
{
	QVBoxLayout *mainLayout = new QVBoxLayout();

	QGridLayout *topLayout = new QGridLayout();

	int row = 0;
	auto add_stat = [&](const char *str) {
		QLabel *label = new QLabel(str, this);
		topLayout->addWidget(label, row, 0);
		label = new QLabel("-", this);
		topLayout->addWidget(label, row++, 1);
		return label;
	};

	r128_short = add_stat(obs_module_text("Label.Short"));
	r128_integrated = add_stat(obs_module_text("Label.Integrated"));
	r128_range = add_stat(obs_module_text("Label.Range"));

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();

	pauseButton = new QPushButton(obs_module_text("Button.Pause"));
	buttonLayout->addWidget(pauseButton);
	connect(pauseButton, &QPushButton::clicked, this, &LoudnessDock::on_pause);

	QPushButton *resetButton = new QPushButton(obs_module_text("Button.Reset"));
	buttonLayout->addWidget(resetButton);
	connect(resetButton, &QPushButton::clicked, this, &LoudnessDock::on_reset);

	mainLayout->addLayout(topLayout);
	mainLayout->addLayout(buttonLayout);
	setLayout(mainLayout);

	loudness = loudness_create(0);

	QTimer *timer = new QTimer(this);
	timer->setInterval(100);
	timer->start(1000);
	connect(timer, &QTimer::timeout, this, &LoudnessDock::on_timer);
}

LoudnessDock::~LoudnessDock()
{
	loudness_destroy(loudness);
}

extern "C" QWidget *create_loudness_dock()
{
	return static_cast<QWidget *>(new LoudnessDock());
}

void LoudnessDock::on_reset()
{
	loudness_reset(loudness);
}

void LoudnessDock::on_pause()
{
	if (paused) {
		if (pauseButton)
			pauseButton->setText(obs_module_text("Button.Pause"));
		paused = false;
	}
	else {
		if (pauseButton)
			pauseButton->setText(obs_module_text("Button.Resume"));
		paused = true;
	}

	loudness_set_pause(loudness, paused);
}

void LoudnessDock::on_timer()
{
	char str[32];
	double results[5];

	if (!loudness)
		return;

	loudness_get(loudness, results);

	snprintf(str, sizeof(str), "%0.1f LUFS", results[1]);
	r128_short->setText(str);

	snprintf(str, sizeof(str), "%0.1f LUFS", results[2]);
	r128_integrated->setText(str);

	snprintf(str, sizeof(str), "%0.1f LU", results[3]);
	r128_range->setText(str);
}
