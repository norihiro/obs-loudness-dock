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
#include <QTabBar>
#include <obs-frontend-api.h>
#include <obs-websocket-api.h>
#include <util/config-file.h>
#ifdef ENABLE_PROFILE
#include <util/profiler.hpp>
#endif
#include "plugin-macros.generated.h"
#include "loudness-dock.hpp"
#include "config-dialog.hpp"
#include "meter.hpp"
#include "utils.hpp"

#define CFG "LoudnessDock"

#define QT_TO_UTF8(str) str.toUtf8().constData()

extern "C" obs_websocket_vendor ws_vendor;

static loudness_dock_config_s load_config()
{
	ASSERT_THREAD(OBS_TASK_UI);

	loudness_dock_config_s cfg;

	config_t *pc = obs_frontend_get_profile_config();

	cfg.abbrev_label = config_get_bool(pc, CFG, "abbrev_label");

	uint32_t n_tabs = config_get_uint(pc, CFG, "n_tabs");
	if (!n_tabs) {
		n_tabs = 1;
		cfg.tabs.resize(1);
		cfg.tabs[0].name = "A";
	}
	else {
		cfg.tabs.resize(n_tabs);

		for (uint32_t i = 0; i < n_tabs; i++) {
			char name[32];
			snprintf(name, sizeof(name), "tab.%d.name", i);
			cfg.tabs[i].name = config_get_string(pc, CFG, name);

			snprintf(name, sizeof(name), "tab.%d.track", i);
			cfg.tabs[i].track = config_get_int(pc, CFG, name);

			snprintf(name, sizeof(name), "tab.%d.trigger", i);
			cfg.tabs[i].trigger_mode =
				(loudness_dock_config_s::trigger_mode_e)config_get_int(pc, CFG, name);
		}
	}

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

	config_set_bool(pc, CFG, "abbrev_label", cfg.abbrev_label);

	config_set_uint(pc, CFG, "n_tabs", cfg.tabs.size());
	for (uint32_t i = 0; i < cfg.tabs.size(); i++) {
		char name[32];
		snprintf(name, sizeof(name), "tab.%d.name", i);
		config_set_string(pc, CFG, name, cfg.tabs[i].name.c_str());

		snprintf(name, sizeof(name), "tab.%d.track", i);
		config_set_int(pc, CFG, name, cfg.tabs[i].track);

		snprintf(name, sizeof(name), "tab.%d.trigger", i);
		config_set_int(pc, CFG, name, (int)cfg.tabs[i].trigger_mode);
	}

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

static const char *pause_resume_button_text(bool paused)
{
	if (paused)
		return obs_module_text("Button.Resume");
	else
		return obs_module_text("Button.Pause");
}

LoudnessDock::LoudnessDock(QWidget *parent) : QFrame(parent)
{
	ASSERT_THREAD(OBS_TASK_UI);

	for (auto &r : results)
		r = -HUGE_VAL;

	QVBoxLayout *mainLayout = new QVBoxLayout();

	tabbar = new QTabBar(this);
	mainLayout->addWidget(tabbar);

	QGridLayout *topLayout = new QGridLayout();

	topLayout->setColumnStretch(3, 1);

	int row = 0;
	auto add_stat = [&](const char *str, QLabel **nameLabel, QLabel **valueLabel, const char *unit,
			    SingleMeter **meter = nullptr) {
		*nameLabel = new QLabel(str, this);
		topLayout->addWidget(*nameLabel, row, 0);

		if (valueLabel) {
			*valueLabel = new QLabel("-", this);
			topLayout->addWidget(*valueLabel, row, 1);
			(*valueLabel)->setAlignment(Qt::AlignRight);

			QFontMetrics metrics((*valueLabel)->font());
			QRect bounds = metrics.boundingRect(QStringLiteral("%1").arg(-199.0, 2, 'f', 1));
			(*valueLabel)->setMinimumWidth(bounds.width());

			auto *unitLabel = new QLabel(QString(unit));
			topLayout->addWidget(unitLabel, row, 2);
			unitLabel->setMinimumWidth(bounds.width());
		}

		if (meter) {
			*meter = new SingleMeter(this);
			topLayout->addWidget(*meter, row, 3);
		}

		row++;
	};

	add_stat(obs_module_text("Label.Momentary"), &label_momentary, &r128_momentary, "LUFS", &meter_momentary);
	add_stat(obs_module_text("Label.Short"), &label_short, &r128_short, "LUFS", &meter_short);
	add_stat(obs_module_text("Label.Integrated"), &label_integrated, &r128_integrated, "LUFS", &meter_integrated);
	add_stat(obs_module_text("Label.Range"), &label_range, &r128_range, "LU");
	add_stat(obs_module_text("Label.Peak"), &label_peak, &r128_peak, "dB<sub>TP</sub>");

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

	connect(tabbar, &QTabBar::currentChanged, this, &LoudnessDock::on_tabbar_changed);

	obs_frontend_add_event_callback(LoudnessDock::on_frontend_event, this);

	QTimer *timer = new QTimer(this);
	timer->start(24);
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

	if (!frontend_exited)
		obs_frontend_remove_event_callback(LoudnessDock::on_frontend_event, this);

	if (ws_vendor) {
		obs_websocket_vendor_unregister_request(ws_vendor, "get_loudness");
		obs_websocket_vendor_unregister_request(ws_vendor, "reset");
		obs_websocket_vendor_unregister_request(ws_vendor, "pause");
	}

	for (loudness_t *loudness : ll)
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

	loudness_t *loudness = get();
	if (!loudness)
		return;

	loudness_reset(loudness);
	update_count = 0;
}

void LoudnessDock::on_tabbar_changed(int ix)
{
	ASSERT_THREAD(OBS_TASK_UI);

	ix_ll = ix;

	update_pause_button();

	update_count = 0;
	QMetaObject::invokeMethod(this, [this](){ on_timer(); }, Qt::QueuedConnection);
}

void LoudnessDock::update_pause_button()
{
	ASSERT_THREAD(OBS_TASK_UI);

	loudness_t *loudness = get();
	if (!loudness) {
		blog(LOG_ERROR, "%s: loudness is NULL. ix=%d", __func__, ix_ll);
		return;
	}

	paused = loudness_paused(loudness);
	pauseButton->setText(pause_resume_button_text(paused));
}

void LoudnessDock::on_pause(bool pause_)
{
	ASSERT_THREAD(OBS_TASK_UI);

	loudness_t *loudness = get();
	if (!loudness)
		return;

	if (pauseButton) {
		pauseButton->setText(pause_resume_button_text(pause_));
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
#ifdef ENABLE_PROFILE
	ScopeProfiler profiler(__func__);
#endif
	ASSERT_THREAD(OBS_TASK_UI);

	uint32_t flags = LOUDNESS_GET_SHORT;

	loudness_t *loudness = get();
	if (!loudness)
		return;

	if (update_count % 2 == 0)
		flags |= LOUDNESS_GET_LONG;

	if (update_count >= 15)
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
		if (update_count % 4 == 0) {
			/* TECH 3341 requires to update the short-term loudness at least 10 Hz. */
			r128_momentary->setText(QStringLiteral("%1").arg(results[0], 2, 'f', 1));
			r128_short->setText(QStringLiteral("%1").arg(results[1], 2, 'f', 1));
		}

		meter_momentary->setLevel(results[0]);
		meter_short->setLevel(results[1]);
	}
	if (flags & LOUDNESS_GET_LONG) {
		if (update_count % 16 == 1) {
			/* TECH 3341 requires to update at least 1 Hz. */
			r128_integrated->setText(QStringLiteral("%1").arg(results[2], 2, 'f', 1));
		}
		r128_range->setText(QStringLiteral("%1").arg(results[3], 2, 'f', 1));
		r128_peak->setText(QStringLiteral("%1").arg(results[4], 2, 'f', 1));

		meter_integrated->setLevel(results[2]);
	}
}

void LoudnessDock::on_config()
{
	ASSERT_THREAD(OBS_TASK_UI);

	if (dialog) {
		dialog->raise();
		return;
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

	if (config.abbrev_label && !cfg.abbrev_label) {
		label_momentary->setText(obs_module_text("Label.Momentary"));
		label_short->setText(obs_module_text("Label.Short"));
		label_integrated->setText(obs_module_text("Label.Integrated"));
		label_range->show();
		label_peak->show();
	}
	else if (!config.abbrev_label && cfg.abbrev_label) {
		label_momentary->setText("M");
		label_short->setText("S");
		label_integrated->setText("I");
		label_range->hide();
		label_peak->hide();
	}

	std::unique_lock<std::mutex> lock(results_mutex);

	for (uint32_t i = 0; i < cfg.tabs.size() && (int)cfg.tabs.size() > tabbar->count(); i++) {
		const auto &tab = cfg.tabs[i];
		if (i >= cfg.tabs.size() || tab.name != QT_TO_UTF8(tabbar->tabText(i))) {
			tabbar->insertTab(i, QString::fromStdString(tab.name));
			ll.insert(ll.begin() + i, loudness_create(tab.track));
		}
	}
	for (uint32_t i = 0; (int)i < tabbar->count() && (int)cfg.tabs.size() < tabbar->count();) {
		if (i >= cfg.tabs.size() || cfg.tabs[i].name != QT_TO_UTF8(tabbar->tabText(i))) {
			tabbar->removeTab(i);
			loudness_t *l = ll[i];
			ll.erase(ll.begin() + i);
			loudness_destroy(l);
		}
		else
			i++;
	}
	if ((int)cfg.tabs.size() != tabbar->count()) {
		blog(LOG_ERROR,
		     "Expected the tabbar size is same as the config but tabbar has %d items, "
		     "cfg.tabs has %zu items.",
		     tabbar->count(), cfg.tabs.size());
	}
	else {
		for (uint32_t i = 0; i < cfg.tabs.size(); i++) {
			const auto &tab = cfg.tabs[i];

			if (tab.name != QT_TO_UTF8(tabbar->tabText(i))) {
				tabbar->setTabText(i, QString::fromStdString(tab.name));
			}

			if (tab.track != loudness_track(ll[i])) {
				loudness_t *l = ll[i];
				ll[i] = loudness_create(tab.track);
				loudness_destroy(l);
			}
		}
	}

	if (cfg.tabs.size() <= 1)
		tabbar->hide();
	else
		tabbar->show();

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
				 (uint32_t)cfg.bar_fg_colors.size());
	}

	config = std::move(cfg);
}

template<typename F> void run_functor(void *data)
{
	auto &fp = *static_cast<F *>(data);
	fp();
}

template<typename F> void run_in_ui_and_wait(F f)
{
	if (obs_in_task_thread(OBS_TASK_UI))
		f();
	else
		obs_queue_task(OBS_TASK_UI, run_functor<F>, &f, true);
}

void LoudnessDock::ws_get_loudness_cb(obs_data_t *request, obs_data_t *response, void *priv_data)
{
	auto ld = static_cast<LoudnessDock *>(priv_data);
	run_in_ui_and_wait([ld, request, response]() { ld->ws_get_loudness_cb(request, response); });
}

static void ws_loudness_set_response(obs_data_t *response, double results[5])
{
	obs_data_set_double(response, "momentary", results[0]);
	obs_data_set_double(response, "short", results[1]);
	obs_data_set_double(response, "integrated", results[2]);
	obs_data_set_double(response, "range", results[3]);
	obs_data_set_double(response, "peak", results[4]);
}

void LoudnessDock::ws_get_loudness_cb(obs_data_t *request, obs_data_t *response)
{
	ASSERT_THREAD(OBS_TASK_UI);

	if (loudness_t *loudness = get_by_name_in_data(request)) {
		double res[5];
		loudness_get(loudness, res, LOUDNESS_GET_SHORT | LOUDNESS_GET_LONG);
		ws_loudness_set_response(response, res);
		return;
	}

	std::unique_lock<std::mutex> lock(results_mutex);
	ws_loudness_set_response(response, results);
}

void LoudnessDock::ws_reset_cb(obs_data_t *request, obs_data_t *, void *priv_data)
{
	auto ld = static_cast<LoudnessDock *>(priv_data);

	run_in_ui_and_wait([ld, request]() {
		if (loudness_t *loudness = ld->get_by_name_in_data(request))
			loudness_reset(loudness);
		else
			ld->on_reset();
	});
}

void LoudnessDock::ws_pause_cb(obs_data_t *request, obs_data_t *, void *priv_data)
{
	auto ld = static_cast<LoudnessDock *>(priv_data);

	run_in_ui_and_wait([ld, request]() {
		bool p = true;
		if (obs_data_has_user_value(request, "pause") && !obs_data_get_bool(request, "pause"))
			p = false;

		if (loudness_t *loudness = ld->get_by_name_in_data(request)) {
			loudness_set_pause(loudness, p);

			/* For the case the selected loudness is paused/resumed, */
			ld->update_pause_button();
		}
		else {
			ld->on_pause(p);
		}
	});
}

void LoudnessDock::on_frontend_event(enum obs_frontend_event event)
{
	if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) {
		auto cfg = load_config();
		apply_move_config(cfg);
	}
	else if (event == OBS_FRONTEND_EVENT_EXIT) {
		frontend_exited = true;
		obs_frontend_remove_event_callback(LoudnessDock::on_frontend_event, this);
	}

	bool streaming_updated = false;
	bool recording_updated = false;
	uint32_t next_state = streaming_recording_state;
	if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		next_state |= loudness_dock_config_s::trigger_streaming;
		streaming_updated = true;
	}
	else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPING) {
		next_state &= ~loudness_dock_config_s::trigger_streaming;
		streaming_updated = true;
	}
	else if (event == OBS_FRONTEND_EVENT_RECORDING_STARTED) {
		next_state |= loudness_dock_config_s::trigger_recording;
		recording_updated = true;
	}
	else if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPING) {
		next_state &= ~loudness_dock_config_s::trigger_recording;
		recording_updated = true;
	}
	else if (event == OBS_FRONTEND_EVENT_RECORDING_UNPAUSED) {
		// next_state |= loudness_dock_config_s::trigger_recording;
		recording_paused = false;
		recording_updated = true;
	}
	else if (event == OBS_FRONTEND_EVENT_RECORDING_PAUSED) {
		// next_state &= ~loudness_dock_config_s::trigger_recording;
		recording_paused = true;
		recording_updated = true;
	}

	if (streaming_updated || recording_updated) {
		ASSERT_THREAD(OBS_TASK_UI);
		bool updated = false;
		for (int i = 0; i < (int)ll.size(); i++) {
			if (i >= (int)config.tabs.size())
				continue;

			const auto trigger_mode = config.tabs[i].trigger_mode;
			if (streaming_updated && !(trigger_mode & loudness_dock_config_s::trigger_streaming))
				continue;
			if (recording_updated && !(trigger_mode & loudness_dock_config_s::trigger_recording))
				continue;

			if ((trigger_mode & streaming_recording_state) == 0 && (trigger_mode & next_state) != 0) {
				loudness_reset(ll[i]);
			}

			auto state_for_pause = next_state;
			if (recording_paused)
				state_for_pause &= ~loudness_dock_config_s::trigger_recording;

			if (trigger_mode & state_for_pause) {
				loudness_set_pause(ll[i], false);
			}
			else {
				bool was_paused = loudness_paused(ll[i]);
				loudness_set_pause(ll[i], true);

				if (!was_paused) {
					double res[5];
					loudness_get(ll[i], res, LOUDNESS_GET_SHORT | LOUDNESS_GET_LONG);
					blog(LOG_INFO, "name='%s' track=%d M=%0.1f S=%0.1f I=%0.1f R=%0.1f P=%0.1f",
					     config.tabs[i].name.c_str(), config.tabs[i].track, res[0], res[1], res[2], res[3],
					     res[4]);
				}
			}
			updated = true;
		}

		if (updated) {
			update_count = 0;
			update_pause_button();
		}
		streaming_recording_state = next_state;
	}
}

void LoudnessDock::on_frontend_event(enum obs_frontend_event event, void *data)
{
	auto ld = static_cast<LoudnessDock *>(data);
	ld->on_frontend_event(event);
}

loudness_t *LoudnessDock::get_by_name(const char *name)
{
	ASSERT_THREAD(OBS_TASK_UI);

	std::unique_lock<std::mutex> lock(results_mutex);

	for (size_t i = 0; i < config.tabs.size(); i++) {
		if (config.tabs[i].name == name)
			return ll[i];
	}

	return nullptr;
}

loudness_t *LoudnessDock::get_by_name_in_data(obs_data_t *request)
{
	const char *name = obs_data_get_string(request, "name");
	if (name)
		return get_by_name(name);

	return nullptr;
}
