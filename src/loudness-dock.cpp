/*
OBS Loudness Dock
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
#include <QMainWindow>
#include <obs-frontend-api.h>
#include <obs-websocket-api.h>
#include "plugin-macros.generated.h"
#include "loudness-dock.hpp"

extern "C" obs_websocket_vendor ws_vendor;

LoudnessDock::LoudnessDock(QWidget *parent) : QFrame(parent)
{
	for (auto &r : results)
		r = -HUGE_VAL;

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

	r128_momentary = add_stat(obs_module_text("Label.Momentary"));
	r128_short = add_stat(obs_module_text("Label.Short"));
	r128_integrated = add_stat(obs_module_text("Label.Integrated"));
	r128_range = add_stat(obs_module_text("Label.Range"));
	r128_peak = add_stat(obs_module_text("Label.Peak"));

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();

	pauseButton = new QPushButton(obs_module_text("Button.Pause"), this);
	buttonLayout->addWidget(pauseButton);
	connect(pauseButton, &QPushButton::clicked, this, &LoudnessDock::on_pause_resume);

	QPushButton *resetButton = new QPushButton(obs_module_text("Button.Reset"), this);
	buttonLayout->addWidget(resetButton);
	connect(resetButton, &QPushButton::clicked, this, &LoudnessDock::on_reset);

	mainLayout->addLayout(topLayout);
	mainLayout->addLayout(buttonLayout);
	setLayout(mainLayout);

	loudness = loudness_create(0);

	QTimer *timer = new QTimer(this);
	timer->start(100);
	connect(timer, &QTimer::timeout, this, &LoudnessDock::on_timer);

	/*
	 * Register an obs-websocket request handler. This assumes `LoudnessDock` is instantiated only once.
	 */
	if (ws_vendor) {
		obs_websocket_vendor_register_request(ws_vendor, "get_loudness", ws_get_loudness_cb, this);
		obs_websocket_vendor_register_request(ws_vendor, "reset", ws_reset_cb, this);
		obs_websocket_vendor_register_request(ws_vendor, "pause", ws_pause_cb, this);
	}
}

LoudnessDock::~LoudnessDock()
{
	if (ws_vendor) {
		obs_websocket_vendor_unregister_request(ws_vendor, "get_loudness");
		obs_websocket_vendor_unregister_request(ws_vendor, "reset");
		obs_websocket_vendor_unregister_request(ws_vendor, "pause");
	}

	loudness_destroy(loudness);
}

extern "C" QWidget *create_loudness_dock()
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return static_cast<QWidget *>(new LoudnessDock(main_window));
}

void LoudnessDock::on_reset()
{
	loudness_reset(loudness);
	update_count = 0;
}

void LoudnessDock::on_pause(bool pause_)
{
	if (pauseButton) {
		if (pause_)
			pauseButton->setText(obs_module_text("Button.Resume"));
		else
			pauseButton->setText(obs_module_text("Button.Pause"));
	}
	paused = pause_;

	loudness_set_pause(loudness, pause_);
	update_count = 0;
}

void LoudnessDock::on_pause_resume()
{
	on_pause(!paused);
}

void LoudnessDock::on_timer()
{
	uint32_t flags = LOUDNESS_GET_SHORT;

	if (!loudness)
		return;

	if (update_count == 0)
		flags |= LOUDNESS_GET_LONG;

	if (update_count >= 9)
		update_count = 0;
	else
		update_count++;

	std::unique_lock<std::mutex> lock(results_mutex);

	loudness_get(loudness, results, flags);

	for (auto &r : results) {
		if (r < -192.0)
			r = -HUGE_VAL;
	}

	lock.unlock();

	if (flags & LOUDNESS_GET_SHORT) {
		r128_momentary->setText(QStringLiteral("%1 LUFS").arg(results[0], 2, 'f', 1));
		r128_short->setText(QStringLiteral("%1 LUFS").arg(results[1], 2, 'f', 1));
	}
	if (flags & LOUDNESS_GET_LONG) {
		r128_integrated->setText(QStringLiteral("%1 LUFS").arg(results[2], 2, 'f', 1));
		r128_range->setText(QStringLiteral("%1 LU").arg(results[3], 2, 'f', 1));
		r128_peak->setText(QStringLiteral("%1 dB<sub>TP</sub>").arg(results[4], 2, 'f', 1));
	}
}

void LoudnessDock::ws_get_loudness_cb(obs_data_t *, obs_data_t *response, void *priv_data)
{
	auto ld = static_cast<LoudnessDock *>(priv_data);
	ld->ws_get_loudness_cb(response);
}

void LoudnessDock::ws_get_loudness_cb(obs_data_t *response)
{
	std::unique_lock<std::mutex> lock(results_mutex);
	obs_data_set_double(response, "momentary", results[0]);
	obs_data_set_double(response, "short", results[1]);
	obs_data_set_double(response, "integrated", results[2]);
	obs_data_set_double(response, "range", results[3]);
	obs_data_set_double(response, "peak", results[4]);
}

void LoudnessDock::ws_reset_cb(obs_data_t *, obs_data_t *, void *priv_data)
{
	auto ld = static_cast<LoudnessDock *>(priv_data);
	ld->on_reset();
}

void LoudnessDock::ws_pause_cb(obs_data_t *request, obs_data_t *, void *priv_data)
{
	auto ld = static_cast<LoudnessDock *>(priv_data);
	bool p = true;
	if (obs_data_has_user_value(request, "pause") && !obs_data_get_bool(request, "pause"))
		p = false;
	ld->on_pause(p);
}
