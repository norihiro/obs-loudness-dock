#pragma once
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include "loudness.h"

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

private: /* for EBU R 128 processing */
	loudness_t *loudness = nullptr;
	uint32_t update_count = 0;

private:
	void on_reset();
	void on_pause();
	void on_timer();
};
