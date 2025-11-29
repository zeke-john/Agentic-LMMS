/*
 * AgentSettingsDialog.h - Settings dialog for AI Agent
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#ifndef LMMS_GUI_AGENT_SETTINGS_DIALOG_H
#define LMMS_GUI_AGENT_SETTINGS_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace lmms::gui
{

class AgentSettingsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit AgentSettingsDialog(QWidget* parent = nullptr);
	~AgentSettingsDialog() override = default;

private slots:
	void onSaveClicked();
	void onApiKeyChanged(const QString& text);
	void onModelsReply(QNetworkReply* reply);

private:
	void loadCurrentSettings();
	void populateModelList();

	QLineEdit* m_apiKeyEdit;
	QComboBox* m_modelCombo;
	QPushButton* m_saveButton;
	QPushButton* m_cancelButton;
	QLabel* m_statusLabel;
	QNetworkAccessManager* m_networkManager;
};

} // namespace lmms::gui

#endif // LMMS_GUI_AGENT_SETTINGS_DIALOG_H

