/*
Loudness Dock for OBS studio
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
#include <QVariant>
#include <QTimer>
#include <QMainWindow>
#include <obs-frontend-api.h>
#include <obs-websocket-api.h>
#include <util/config-file.h>
#include "plugin-macros.generated.h"
#include "loudness-dock.hpp"
#include "config-dialog.hpp"
#include "meter.hpp"
#include "utils.hpp"

#define CFG "LoudnessDock"

extern "C" obs_websocket_vendor ws_vendor;

static loudness_dock_config_s load_config()
{
	ASSERT_THREAD(OBS_TASK_UI);

	loudness_dock_config_s cfg;

	config_t *pc = obs_frontend_get_profile_config();

	uint32_t n_colors = config_get_uint(pc, CFG, "n_colors");
	if (!n_colors) {
		n_colors = 3;

		cfg.bar_fg_colors.resize(3);
		cfg.bar_fg_colors[0] = 0x00FF0000;
		cfg.bar_fg_colors[1] = 0x0000FF00;
		cfg.bar_fg_colors[2] = 0x000000FF;

		cfg.bar_bg_colors.resize(3);
		cfg.bar_bg_colors[0] = 0x00550000;
		cfg.bar_bg_colors[1] = 0x00005500;
		cfg.bar_bg_colors[2] = 0x00000055;

		cfg.bar_thresholds.resize(2);
		cfg.bar_thresholds[0] = -23.0;
		cfg.bar_thresholds[1] = -14.0;
	}
	else {
		cfg.bar_fg_colors.resize(n_colors);
		cfg.bar_bg_colors.resize(n_colors);
		cfg.bar_thresholds.resize(n_colors - 1);

		for (uint32_t i = 0; i < n_colors; i++) {
			char name[32];
			snprintf(name, sizeof(name), "color.fg.%d", i);
			cfg.bar_fg_colors[i] = config_get_uint(pc, CFG, name);

			snprintf(name, sizeof(name), "color.bg.%d", i);
			cfg.bar_bg_colors[i] = config_get_uint(pc, CFG, name);
		}

		for (uint32_t i = 0; i < n_colors - 1; i++) {
			char name[32];
			snprintf(name, sizeof(name), "threshold.%d", i);
			cfg.bar_thresholds[i] = (float)config_get_double(pc, CFG, name);
		}
	}

	return cfg;
}

static void save_config(const loudness_dock_config_s &cfg)
{
	ASSERT_THREAD(OBS_TASK_UI);

	config_t *pc = obs_frontend_get_profile_config();

	config_set_uint(pc, CFG, "n_colors", cfg.bar_fg_colors.size());

	for (uint32_t i = 0; i < cfg.bar_fg_colors.size(); i++) {
		char name[32];
		snprintf(name, sizeof(name), "color.fg.%d", i);
		config_set_uint(pc, CFG, name, cfg.bar_fg_colors[i]);
	}

	for (uint32_t i = 0; i < cfg.bar_bg_colors.size(); i++) {
		char name[32];
		snprintf(name, sizeof(name), "color.bg.%d", i);
		config_set_uint(pc, CFG, name, cfg.bar_bg_colors[i]);
	}

	for (uint32_t i = 0; i < cfg.bar_thresholds.size(); i++) {
		char name[32];
		snprintf(name, sizeof(name), "threshold.%d", i);
		config_set_double(pc, CFG, name, cfg.bar_thresholds[i]);
	}

	config_save_safe(pc, "tmp", nullptr);
}

LoudnessDock::LoudnessDock(QWidget *parent) : QFrame(parent)
{
	ASSERT_THREAD(OBS_TASK_UI);

	for (auto &r : results)
		r = -HUGE_VAL;

	QVBoxLayout *mainLayout = new QVBoxLayout();

	QGridLayout *topLayout = new QGridLayout();

	topLayout->setColumnStretch(2, 1);

	int row = 0;
	auto add_stat = [&](const char *str, QLabel **valueLabel, SingleMeter **meter = nullptr) {
		QLabel *label = new QLabel(str, this);
		topLayout->addWidget(label, row, 0);

		if (valueLabel) {
			*valueLabel = new QLabel("-", this);
			topLayout->addWidget(*valueLabel, row, 1);

			QFontMetrics metrics((*valueLabel)->font());
			QRect bounds = metrics.boundingRect(QStringLiteral("%1 LUFS").arg(-199.0, 2, 'f', 1));
			(*valueLabel)->setMinimumWidth(bounds.width());
		}

		if (meter) {
			*meter = new SingleMeter(this);
			topLayout->addWidget(*meter, row, 2);
		}

		row++;
	};

	add_stat(obs_module_text("Label.Momentary"), &r128_momentary, &meter_momentary);
	add_stat(obs_module_text("Label.Short"), &r128_short, &meter_short);
	add_stat(obs_module_text("Label.Integrated"), &r128_integrated, &meter_integrated);
	add_stat(obs_module_text("Label.Range"), &r128_range);
	add_stat(obs_module_text("Label.Peak"), &r128_peak);

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->addStretch();

	pauseButton = new QPushButton(obs_module_text("Button.Pause"), this);
	buttonLayout->addWidget(pauseButton);
	connect(pauseButton, &QPushButton::clicked, this, &LoudnessDock::on_pause_resume);

	QPushButton *resetButton = new QPushButton(obs_module_text("Button.Reset"), this);
	buttonLayout->addWidget(resetButton);
	connect(resetButton, &QPushButton::clicked, this, &LoudnessDock::on_reset);

	QPushButton *configButton = new QPushButton(this);
	if (obs_get_version() >= MAKE_SEMANTIC_VERSION(31, 0, 0))
		configButton->setProperty("class", "btn-tool icon-gear");
	else
		configButton->setText(obs_module_text("Button.Configuration"));
	buttonLayout->addWidget(configButton);
	connect(configButton, &QPushButton::clicked, this, &LoudnessDock::on_config);

	mainLayout->addLayout(topLayout);
	mainLayout->addLayout(buttonLayout);
	setLayout(mainLayout);

	auto cfg = load_config();
	apply_move_config(cfg);

	obs_frontend_add_event_callback(LoudnessDock::on_frontend_event, this);

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
	ASSERT_THREAD(OBS_TASK_UI);

	obs_frontend_remove_event_callback(LoudnessDock::on_frontend_event, this);

	if (ws_vendor) {
		obs_websocket_vendor_unregister_request(ws_vendor, "get_loudness");
		obs_websocket_vendor_unregister_request(ws_vendor, "reset");
		obs_websocket_vendor_unregister_request(ws_vendor, "pause");
	}

	loudness_destroy(loudness);
}

extern "C" QWidget *create_loudness_dock()
{
	ASSERT_THREAD(OBS_TASK_UI);

	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return static_cast<QWidget *>(new LoudnessDock(main_window));
}

void LoudnessDock::on_reset()
{
	ASSERT_THREAD(OBS_TASK_UI);

	loudness_reset(loudness);
	update_count = 0;
}

void LoudnessDock::on_pause(bool pause_)
{
	ASSERT_THREAD(OBS_TASK_UI);

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
	ASSERT_THREAD(OBS_TASK_UI);

	on_pause(!paused);
}

void LoudnessDock::on_timer()
{
	ASSERT_THREAD(OBS_TASK_UI);

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

		meter_momentary->setLevel(results[0]);
		meter_short->setLevel(results[1]);
	}
	if (flags & LOUDNESS_GET_LONG) {
		r128_integrated->setText(QStringLiteral("%1 LUFS").arg(results[2], 2, 'f', 1));
		r128_range->setText(QStringLiteral("%1 LU").arg(results[3], 2, 'f', 1));
		r128_peak->setText(QStringLiteral("%1 dB<sub>TP</sub>").arg(results[4], 2, 'f', 1));

		meter_integrated->setLevel(results[2]);
	}
}

void LoudnessDock::on_config()
{
	ASSERT_THREAD(OBS_TASK_UI);

	if (dialog) {
		delete dialog;
	}

	dialog = new ConfigDialog(config, this);
	connect(dialog, &ConfigDialog::changed, this, &LoudnessDock::on_config_changed);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->show();
}

void LoudnessDock::on_config_changed()
{
	ASSERT_THREAD(OBS_TASK_UI);

	if (!dialog) {
		blog(LOG_ERROR, "on_config_changed: no dialog");
		return;
	}

	loudness_dock_config_s cfg = dialog->getConfig();

	apply_move_config(cfg);

	save_config(config);
}

void LoudnessDock::apply_move_config(loudness_dock_config_s &cfg)
{
	ASSERT_THREAD(OBS_TASK_UI);

	if (cfg.bar_thresholds.size() + 1 != cfg.bar_fg_colors.size()) {
		size_t n_colors;
		if (cfg.bar_thresholds.size() <= 0 || cfg.bar_fg_colors.size() <= 1)
			n_colors = 1;
		else
			n_colors = std::min(cfg.bar_thresholds.size() + 1, cfg.bar_fg_colors.size());

		blog(LOG_ERROR, "bar_thresholds has %zu members, bar_fg_colors has %zu members. Resizing to %zu",
		     cfg.bar_thresholds.size(), cfg.bar_fg_colors.size(), n_colors);

		cfg.bar_thresholds.resize(n_colors - 1);
		cfg.bar_fg_colors.resize(n_colors);
	}

	if (cfg.bar_bg_colors.size() < cfg.bar_fg_colors.size()) {
		blog(LOG_ERROR, "bar_bg_colors has %zu members, where expected %zu. Resizing.",
		     cfg.bar_bg_colors.size(), cfg.bar_fg_colors.size());
		for (size_t i = cfg.bar_bg_colors.size(); i < cfg.bar_fg_colors.size(); i++) {
			uint32_t c = cfg.bar_fg_colors[i];
			uint32_t bg = (c & 0xFEFEFE) >> 1;
			cfg.bar_bg_colors.push_back(bg);
		}
	}

	/* Now, the sizes of bar_thresholds, bar_fg_colors, and bar_bg_colors are consistent. */

	SingleMeter *meters[] = {
		meter_momentary,
		meter_short,
		meter_integrated,
	};

	for (SingleMeter *meter : meters) {
		if (!meter)
			continue;
		meter->setColors(cfg.bar_thresholds.data(), cfg.bar_fg_colors.data(), cfg.bar_bg_colors.data(),
				 cfg.bar_fg_colors.size());
	}

	config = std::move(cfg);
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
	QMetaObject::invokeMethod(ld, [=]() { ld->on_reset(); }, Qt::QueuedConnection);
}

void LoudnessDock::ws_pause_cb(obs_data_t *request, obs_data_t *, void *priv_data)
{
	auto ld = static_cast<LoudnessDock *>(priv_data);
	bool p = true;
	if (obs_data_has_user_value(request, "pause") && !obs_data_get_bool(request, "pause"))
		p = false;
	QMetaObject::invokeMethod(ld, [=]() { ld->on_pause(p); }, Qt::QueuedConnection);
}

void LoudnessDock::on_frontend_event(enum obs_frontend_event event)
{
	if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) {
		auto cfg = load_config();
		apply_move_config(cfg);
	}
}

void LoudnessDock::on_frontend_event(enum obs_frontend_event event, void *data)
{
	auto ld = static_cast<LoudnessDock *>(data);
	ld->on_frontend_event(event);
}
