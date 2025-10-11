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
#include <QCheckBox>
#include <QComboBox>
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
	ASSERT_THREAD(OBS_TASK_UI);

	setWindowTitle("Loudness Dock Configuration");
	auto *mainLayout = new QVBoxLayout(this);

	auto *topLayout = new QGridLayout();
	int row = 0;

	abbrevLabelCheck = new QCheckBox(obs_module_text("Config.AbbrevLabel"), this);
	abbrevLabelCheck->setCheckState(cfg.abbrev_label ? Qt::Checked : Qt::Unchecked);
	connect(abbrevLabelCheck, &QCheckBox::toggled, this, &ConfigDialog::on_abbrev_label_changed);
	topLayout->addWidget(abbrevLabelCheck, row++, 1);

	// Tabs table
	topLayout->addWidget(new QLabel(obs_module_text("Config.Tabs"), this), row, 0);
	tabTable = new QTableWidget(0, 3, this);
	topLayout->addWidget(tabTable, row++, 1);
	QStringList tabTableHeader;
	tabTableHeader << obs_module_text("Config.Tabs.Name") << obs_module_text("Config.Tabs.Track")
		       << obs_module_text("Config.Trigger");
	tabTable->setHorizontalHeaderLabels(tabTableHeader);
	tabTable->setMinimumWidth(tabTable->horizontalHeader()->length() + tabTable->verticalHeader()->width() +
				  tabTable->verticalScrollBar()->width());

	auto *tabTableControlLayout = new QHBoxLayout();
	auto *tabTableAdd = new QPushButton(obs_module_text("Config.Add"), this);
	auto *tabTableDel = new QPushButton(obs_module_text("Config.Remove"), this);
	tabTableControlLayout->addWidget(tabTableAdd);
	tabTableControlLayout->addWidget(tabTableDel);
	topLayout->addLayout(tabTableControlLayout, row++, 1);

	for (uint32_t i = 0; i < cfg.tabs.size(); i++) {
		TabTableAdd(i, cfg.tabs[i]);
	}

	connect(tabTable, &QTableWidget::cellChanged, this, &ConfigDialog::on_tab_table_changed);
	connect(tabTableAdd, &QPushButton::clicked, this, &ConfigDialog::on_tab_table_add);
	connect(tabTableDel, &QPushButton::clicked, this, &ConfigDialog::on_tab_table_remove);

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

	for (uint32_t i = 0; i < cfg.bar_fg_colors.size() && i < cfg.bar_bg_colors.size(); i++) {
		float th = i < cfg.bar_thresholds.size() ? cfg.bar_thresholds[i] : 0.0f;
		ColorTableAdd(i, th, cfg.bar_fg_colors[i], cfg.bar_bg_colors[i]);
	}

	if (colorTable->rowCount())
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
	ASSERT_THREAD(OBS_TASK_UI);

	config = config_orig;
	changed();

	QDialog::reject();
}

void ConfigDialog::TabTableAdd(int ix, const struct loudness_dock_config_s::tab_config &tab)
{
	ASSERT_THREAD(OBS_TASK_UI);

	tabTable->insertRow(ix);

	auto *item = new QTableWidgetItem(QString::fromStdString(tab.name));
	tabTable->setItem(ix, 0, item);

	item = new QTableWidgetItem(QString::number(tab.track));
	tabTable->setItem(ix, 1, item);

	auto *trigger = new QComboBox(tabTable);
	trigger->addItem(obs_module_text("Config.Trigger.None"), loudness_dock_config_s::trigger_none);
	trigger->addItem(obs_module_text("Config.Trigger.Streaming"), loudness_dock_config_s::trigger_streaming);
	trigger->addItem(obs_module_text("Config.Trigger.Recording"), loudness_dock_config_s::trigger_recording);
	trigger->addItem(obs_module_text("Config.Trigger.Both"), loudness_dock_config_s::trigger_both);
	trigger->setCurrentIndex(tab.trigger_mode); /* Assumes the code starts from 0 and no continuous */
	tabTable->setCellWidget(ix, 2, trigger);

	connect(trigger, &QComboBox::currentIndexChanged, [this, trigger, ix](int) {
		tabTable->blockSignals(true);
		config.tabs[ix].trigger_mode = (loudness_dock_config_s::trigger_mode_e)trigger->currentData().toInt();
		tabTable->blockSignals(false);
	});
}

void ConfigDialog::ColorTableAdd(int ix, float threshold, uint32_t color_fg, uint32_t color_bg)
{
	ASSERT_THREAD(OBS_TASK_UI);

	colorTable->insertRow(ix);

	auto *item = new QTableWidgetItem(QString::number(threshold, 'f', 1));
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

void ConfigDialog::on_tab_table_add()
{
	ASSERT_THREAD(OBS_TASK_UI);

	auto *ci = tabTable->currentItem();
	int ix = ci && ci->isSelected() ? ci->row() : tabTable->rowCount();
	if (ix < 0 || (int)config.tabs.size() < ix) {
		blog(LOG_ERROR, "%s: invalid ix=%d", __func__, ix);
		return;
	}

	char next_name = 'A';
	for (const auto &tab : config.tabs) {
		if (tab.name.size() != 1)
			continue;
		char n = tab.name[0];
		if (n < 'A' || 'Z' <= n)
			continue;
		if (n >= next_name)
			next_name = n + 1;
	}

	loudness_dock_config_s::tab_config tab;
	tab.name = next_name;
	config.tabs.insert(config.tabs.begin() + ix, tab);
	TabTableAdd(ix, tab);

	changed();
}

void ConfigDialog::on_tab_table_remove()
{
	ASSERT_THREAD(OBS_TASK_UI);

	if (tabTable->rowCount() <= 1)
		return;

	auto *ci = tabTable->currentItem();
	if (!ci || !ci->isSelected())
		return;

	int ix = ci->row();
	if (ix < 0 || (int)config.tabs.size() <= ix)
		return;

	tabTable->removeRow(ix);
	config.tabs.erase(config.tabs.begin() + ix);

	changed();
}

void ConfigDialog::on_color_table_add()
{
	ASSERT_THREAD(OBS_TASK_UI);

	auto *ci = colorTable->currentItem();
	int ix = ci && ci->isSelected() ? ci->row() : colorTable->rowCount();

	ColorTableAdd(ix, 0, 0, 0);
}

void ConfigDialog::on_color_table_remove()
{
	ASSERT_THREAD(OBS_TASK_UI);

	auto *ci = colorTable->currentItem();
	if (!ci || !ci->isSelected())
		return;

	colorTable->removeRow(ci->row());
	on_color_table_changed(-1, -1);
}

void ConfigDialog::on_abbrev_label_changed(bool checked)
{
	if (config.abbrev_label == checked)
		return;

	config.abbrev_label = checked;
	changed();
}

void ConfigDialog::on_tab_table_changed(int row, int column)
{
	ASSERT_THREAD(OBS_TASK_UI);

	if (row < 0 || (int)config.tabs.size() <= row)
		return;

	if (column == 0) {
		auto *item = tabTable->item(row, 0);
		if (!item)
			return;
		config.tabs[row].name = item->text().toUtf8().constData();
	}

	if (column == 1) {
		auto *item = tabTable->item(row, 1);
		if (!item)
			return;
		bool ok = false;
		int track = item->text().toInt(&ok);
		if (ok && 0 <= track && track < MAX_AUDIO_MIXES)
			config.tabs[row].track = track;
		else
			item->setText(QString::number(config.tabs[row].track));
	}

	changed();
}

void ConfigDialog::on_color_table_changed(int row, int column)
{
	ASSERT_THREAD(OBS_TASK_UI);

	std::vector<item_t> data;

	int n_row = colorTable->rowCount();

	if (n_row < 1)
		return;

	for (int i = 0; i < n_row; i++) {
		item_t item;

		auto *thresholdItem = colorTable->item(i, 0);
		if (!thresholdItem)
			return;
		item.threshold = thresholdItem->text().toDouble();

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

	if (data.size() < 1)
		return;

	config.bar_thresholds.resize(data.size() - 1);
	for (uint32_t i = 0; i + 1 < data.size(); i++)
		config.bar_thresholds[i] = data[i].threshold;

	config.bar_fg_colors.resize(data.size());
	for (uint32_t i = 0; i < data.size(); i++)
		config.bar_fg_colors[i] = data[i].color_fg;

	config.bar_bg_colors.resize(data.size());
	for (uint32_t i = 0; i < data.size(); i++)
		config.bar_bg_colors[i] = data[i].color_bg;

	changed();
}
