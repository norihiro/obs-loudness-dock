#pragma once
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QPointer>
#include <mutex>
#include <obs-frontend-api.h>
#include "loudness.h"
#include "config.hpp"
#include "obs.h"

class LoudnessDock : public QFrame {
	Q_OBJECT

public:
	LoudnessDock(QWidget *parent = nullptr);
	~LoudnessDock();

private:
	QPushButton *pauseButton = nullptr;
	bool paused = 0;

	QLabel *label_momentary = nullptr;
	QLabel *label_short = nullptr;
	QLabel *label_integrated = nullptr;
	QLabel *label_range = nullptr;
	QLabel *label_peak = nullptr;

	QLabel *r128_momentary = nullptr;
	QLabel *r128_short = nullptr;
	QLabel *r128_integrated = nullptr;
	QLabel *r128_range = nullptr;
	QLabel *r128_peak = nullptr;

	class SingleMeter *meter_momentary = nullptr;
	class SingleMeter *meter_short = nullptr;
	class SingleMeter *meter_integrated = nullptr;

	loudness_dock_config_s config;

	QPointer<class ConfigDialog> dialog;

	std::mutex results_mutex;
	double results[5];

private: /* for EBU R 128 processing */
	loudness_t *loudness = nullptr;
	uint32_t update_count = 0;

private:
	void on_reset();
	void on_pause(bool pause);
	void on_pause_resume();
	void on_timer();
	void on_config();
	void on_config_changed();
	void on_frontend_event(enum obs_frontend_event event);

	void apply_move_config(loudness_dock_config_s &cfg);

	static void ws_get_loudness_cb(obs_data_t *, obs_data_t *, void *);
	void ws_get_loudness_cb(obs_data_t *);
	static void ws_reset_cb(obs_data_t *, obs_data_t *, void *);
	static void ws_pause_cb(obs_data_t *, obs_data_t *, void *);

	static void on_frontend_event(enum obs_frontend_event event, void *);
};
