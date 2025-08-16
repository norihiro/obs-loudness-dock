#pragma once
#include <QWidget>

class SingleMeter : public QWidget {
	Q_OBJECT

public:
	SingleMeter(QWidget *parent = nullptr);
	~SingleMeter();

	void setRange(double min, double max);
	void setColors(const double *levels, const uint32_t *colors, int size);
	void setLevel(double level);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	struct private_data &data;
};
