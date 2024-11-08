#pragma once
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <mutex>
#include "loudness.h"
#include "obs.h"

class LoudnessDock : public QFrame {
	Q_OBJECT

public:
	LoudnessDock(QWidget *parent = nullptr);
	~LoudnessDock();

private:
	QPushButton *pauseButton = nullptr;
	bool paused = 0;

	QLabel *r128_momentary = nullptr;
	QLabel *r128_short = nullptr;
	QLabel *r128_integrated = nullptr;
	QLabel *r128_range = nullptr;
	QLabel *r128_peak = nullptr;

	std::mutex results_mutex;
	double results[5];

private: /* for EBU R 128 processing */
	loudness_t *loudness = nullptr;
	uint32_t update_count = 0;

private:
	void on_reset();
	void on_pause(bool pause);
	void on_resume();
	void on_pause_resume();
	void on_timer();

	static void ws_get_loudness_cb(obs_data_t *, obs_data_t *, void *);
	void ws_get_loudness_cb(obs_data_t *);
	static void ws_reset_cb(obs_data_t *, obs_data_t *, void *);
	static void ws_pause_cb(obs_data_t *, obs_data_t *, void *);
};
