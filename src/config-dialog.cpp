#include <obs-module.h>
#include <cstdlib>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollBar>
#include "plugin-macros.generated.h"
#include "config-dialog.hpp"
#include "config-dialog-table-delegate.hpp"
#include "utils.hpp"

struct item_s
{
	float threshold;
	uint32_t color_fg;
	uint32_t color_bg;
	bool operator<(const item_s &x) const { return threshold < x.threshold; }
};

typedef struct item_s item_t;

ConfigDialog::ConfigDialog(const loudness_dock_config_s &cfg, QWidget *parent)
	: QDialog(parent), config(cfg), config_orig(cfg)
{
	setWindowTitle("Loudness Dock Configuration");
	auto *mainLayout = new QVBoxLayout(this);

	auto *topLayout = new QGridLayout();
	int row = 0;

	// Color table
	topLayout->addWidget(new QLabel(obs_module_text("Config.Colors"), this), row, 0);
	colorTable = new QTableWidget(0, 3, this);
	topLayout->addWidget(colorTable, row++, 1);
	colorTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
	colorTable->setItemDelegateForColumn(0, new ThresholdSpinDelegate(colorTable));
	QStringList colorTableHeader;
	colorTableHeader << obs_module_text("Config.Colors.Threshold") << obs_module_text("Config.Colors.FGColor")
			 << obs_module_text("Config.Colors.BGColor");
	colorTable->setHorizontalHeaderLabels(colorTableHeader);
	colorTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	colorTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	for (uint32_t i = 0; i < cfg.bar_fg_colors.size() and i < cfg.bar_bg_colors.size(); i++) {
		float th = i < cfg.bar_thresholds.size() ? cfg.bar_thresholds[i] : 0.0f;
		ColorTableAdd(i, th, cfg.bar_fg_colors[i], cfg.bar_bg_colors[i]);
	}

	colorTable->setMinimumHeight(colorTable->rowHeight(0) * 5);
	colorTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	colorTable->setMinimumWidth(colorTable->horizontalHeader()->length() + colorTable->verticalHeader()->width() +
				    colorTable->verticalScrollBar()->width());

	auto *colorTableControlLayout = new QHBoxLayout();
	auto *colorTableAdd = new QPushButton(obs_module_text("Config.Add"), this);
	auto *colorTableDel = new QPushButton(obs_module_text("Config.Remove"), this);
	colorTableControlLayout->addWidget(colorTableAdd);
	colorTableControlLayout->addWidget(colorTableDel);
	topLayout->addLayout(colorTableControlLayout, row++, 1);
	connect(colorTable, &QTableWidget::cellChanged, this, &ConfigDialog::on_color_table_changed);
	connect(colorTableAdd, &QPushButton::clicked, this, &ConfigDialog::on_color_table_add);
	connect(colorTableDel, &QPushButton::clicked, this, &ConfigDialog::on_color_table_remove);

	// Generic buttons
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(this, &QDialog::accepted, this, &ConfigDialog::changed);

	mainLayout->addLayout(topLayout);
	mainLayout->addWidget(buttons, 0, Qt::AlignRight);
}

void ConfigDialog::reject()
{
	config = config_orig;
	changed();

	QDialog::reject();
}

void ConfigDialog::ColorTableAdd(int ix, float threshold, uint32_t color_fg, uint32_t color_bg)
{
	colorTable->insertRow(ix);
	char text[32];

	snprintf(text, sizeof(text), "%0.1f", threshold);
	auto *item = new QTableWidgetItem(text);
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
	colorTable->setItem(ix, 0, item);

	auto qc_fg = color_from_int(color_fg);
	item = new QTableWidgetItem(qc_fg.name(QColor::HexRgb));
	item->setBackground(QBrush(qc_fg));
	colorTable->setItem(ix, 1, item);

	auto qc_bg = color_from_int(color_bg);
	item = new QTableWidgetItem(qc_bg.name(QColor::HexRgb));
	item->setBackground(QBrush(qc_bg));
	colorTable->setItem(ix, 2, item);
}

void ConfigDialog::on_color_table_add()
{
	auto *ci = colorTable->currentItem();
	int ix = ci && ci->isSelected() ? ci->row() : colorTable->rowCount();

	ColorTableAdd(ix, 0, 0, 0);
}

void ConfigDialog::on_color_table_remove()
{
	auto *ci = colorTable->currentItem();
	if (!ci || !ci->isSelected())
		return;

	colorTable->removeRow(ci->row());
	on_color_table_changed(-1, -1);
}

void ConfigDialog::on_color_table_changed(int row, int column)
{
	std::vector<item_t> data;

	int n_row = colorTable->rowCount();

	if (n_row < 1)
		return;

	for (int i = 0; i < n_row; i++) {
		item_t item;

		item.threshold = colorTable->item(i, 0)->text().toDouble();

		auto *fgColorItem = colorTable->item(i, 1);
		if (!fgColorItem)
			return;
		item.color_fg = color_int_from_text(fgColorItem->text().toUtf8().constData());

		auto *bgColorItem = colorTable->item(i, 2);
		if (!bgColorItem)
			return;
		item.color_bg = color_int_from_text(bgColorItem->text().toUtf8().constData());

		if (i == row && column == 1)
			fgColorItem->setBackground(QBrush(color_from_int(item.color_fg)));

		if (i == row && column == 2)
			bgColorItem->setBackground(QBrush(color_from_int(item.color_bg)));

		data.push_back(item);
	}

	sort(data.begin(), data.end());

	config.bar_thresholds.resize(data.size() - 1);
	for (uint32_t i = 0; i < data.size(); i++)
		config.bar_thresholds[i] = data[i].threshold;

	config.bar_fg_colors.resize(data.size());
	for (uint32_t i = 0; i < data.size(); i++)
		config.bar_fg_colors[i] = data[i].color_fg;

	config.bar_bg_colors.resize(data.size());
	for (uint32_t i = 0; i < data.size(); i++)
		config.bar_bg_colors[i] = data[i].color_bg;

	changed();
}
