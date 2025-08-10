#pragma once
#include <QWidget>

class SingleMeter : public QWidget {
	Q_OBJECT

public:
	SingleMeter(QWidget *parent = nullptr);
	~SingleMeter();

	void setRange(float min, float max);
	void setColors(const float *levels, const uint32_t *fg_colors, const uint32_t *bg_colors, uint32_t n_colors);
	void setLevel(float level);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	struct private_data &data;
};
