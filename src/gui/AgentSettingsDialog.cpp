/*
 * AgentSettingsDialog.cpp - Settings dialog for AI Agent
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "AgentSettingsDialog.h"
#include "AgentManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QIODevice>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace lmms::gui
{

AgentSettingsDialog::AgentSettingsDialog(QWidget* parent)
	: QDialog(parent)
	, m_networkManager(new QNetworkAccessManager(this))
{
	setWindowTitle(tr("Settings"));
	setFixedWidth(380);
	
	connect(m_networkManager, &QNetworkAccessManager::finished,
			this, &AgentSettingsDialog::onModelsReply);
	
	QString svgPath = QCoreApplication::applicationDirPath() + "/../plugins/Vibed/downArrow.svg";
	if (!QFileInfo::exists(svgPath))
	{
		svgPath = QCoreApplication::applicationDirPath() + "/../../plugins/Vibed/downArrow.svg";
	}

	QString comboBoxArrowStyle;
	if (QFileInfo::exists(svgPath))
	{
		svgPath = QDir::toNativeSeparators(svgPath);
		comboBoxArrowStyle = QString(
			"QComboBox::down-arrow {"
			"  image: url('%1');"
			"  width: 14px;"
			"  height: 14px;"
			"  margin-right: 10px;"
			"}"
		).arg(svgPath);
	}

	
	QString baseStyleSheet = QString(
		"QDialog {"
		"  background-color: #0d0d0d;"
		"}"
		"QLabel {"
		"  color: #909090;"
		"  font-size: 12px;"
		"}"
		"QLineEdit {"
		"  background-color: #1a1a1a;"
		"  color: #e0e0e0;"
		"  border: none;"
		"  border-radius: 8px;"
		"  padding: 10px 12px;"
		"  font-size: 12px;"
		"}"
		"QLineEdit:focus {"
		"  background-color: #222222;"
		"}"
		"QComboBox {"
		"  background-color: #1a1a1a;"
		"  color: #e0e0e0;"
		"  border: none;"
		"  border-radius: 8px;"
		"  padding: 10px 12px;"
		"  font-size: 12px;"
		"}"
		"QComboBox::drop-down {"
		"  subcontrol-origin: padding;"
		"  subcontrol-position: center right;"
		"  width: 32px;"
		"  border: none;"
		"  background: transparent;"
		"}"
	);
	
	QString restOfStyleSheet = QString(
		"QComboBox QAbstractItemView {"
		"  background-color: #151515;"
		"  color: #e0e0e0;"
		"  border: 1px solid #2a2a2a;"
		"  border-radius: 8px;"
		"  outline: none;"
		"  selection-background-color: #1f1f1f;"
		"  selection-color: #f0f0f0;"
		"  padding: 4px;"
		"}"
		"QComboBox QAbstractItemView::item {"
		"  padding: 8px 12px;"
		"  border: none;"
		"  background: transparent;"
		"}"
		"QComboBox QAbstractItemView::item:hover {"
		"  background: transparent;"
		"}"
		"QComboBox QAbstractItemView::item:selected {"
		"  background-color: #1f1f1f;"
		"  border-radius: 4px;"
		"}"
		"QPushButton {"
		"  background-color: #1a1a1a;"
		"  color: #808080;"
		"  border: none;"
		"  border-radius: 8px;"
		"  padding: 10px 20px;"
		"  font-size: 12px;"
		"}"
		"QPushButton:hover {"
		"  background-color: #2a2a2a;"
		"}"
	);
	
	setStyleSheet(baseStyleSheet + comboBoxArrowStyle + restOfStyleSheet);
	
	

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setSpacing(12);
	mainLayout->setContentsMargins(16, 16, 16, 16);

	// config section
	auto* configLabel = new QLabel(tr("Config"), this);
	configLabel->setStyleSheet("color: white; font-size: 12px;");
	mainLayout->addWidget(configLabel);

	// api key
	m_apiKeyEdit = new QLineEdit(this);
	m_apiKeyEdit->setEchoMode(QLineEdit::Password);
	m_apiKeyEdit->setPlaceholderText(tr("OpenRouter API key..."));
	connect(m_apiKeyEdit, &QLineEdit::textChanged, this, &AgentSettingsDialog::onApiKeyChanged);
	mainLayout->addWidget(m_apiKeyEdit);

	// link 2 get a api get
	auto* helpLabel = new QLabel(
		tr("<a href='https://openrouter.ai/keys' style='color:rgb(84, 181, 255); text-decoration: underline;'>Get a API key from OpenRouter :)</a>"),
		this
	);
	helpLabel->setOpenExternalLinks(true);
	helpLabel->setStyleSheet("font-size: 11px; color: #505050; margin-bottom: 4px;");
	mainLayout->addWidget(helpLabel);

	// model dropdown
	m_modelCombo = new QComboBox(this);
	populateModelList();
	mainLayout->addWidget(m_modelCombo);

	m_statusLabel = new QLabel(this);
	m_statusLabel->setStyleSheet("color: #505050; font-size: 11px; margin-top: 4px;");
	mainLayout->addWidget(m_statusLabel);

	mainLayout->addStretch();

	auto* buttonLayout = new QHBoxLayout();
	buttonLayout->setSpacing(8);

	m_cancelButton = new QPushButton(tr("Cancel"), this);
	connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	buttonLayout->addWidget(m_cancelButton);

	buttonLayout->addStretch();

	m_saveButton = new QPushButton(tr("Save"), this);
	connect(m_saveButton, &QPushButton::clicked, this, &AgentSettingsDialog::onSaveClicked);
	buttonLayout->addWidget(m_saveButton);
	mainLayout->addLayout(buttonLayout);

	loadCurrentSettings();
}

void AgentSettingsDialog::populateModelList()
{
	QUrl url("https://openrouter.ai/api/v1/models");
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	m_networkManager->get(request);
}

void AgentSettingsDialog::onModelsReply(QNetworkReply* reply)
{
	reply->deleteLater();
	
	if (reply->error() != QNetworkReply::NoError)
	{
		return;
	}
	
	QByteArray responseData = reply->readAll();
	QJsonDocument doc = QJsonDocument::fromJson(responseData);
	
	if (doc.isNull() || !doc.isObject())
	{
		return;
	}
	
	QJsonObject root = doc.object();
	QJsonArray data = root["data"].toArray();
	
	QStringList allowedProviders = {"openai", "google", "anthropic", "moonshot"};
	
	for (const QJsonValue& value : data)
	{
		QJsonObject model = value.toObject();
		QString id = model["id"].toString();
		
		QString provider = id.split("/").first();
		if (!allowedProviders.contains(provider))
		{
			continue;
		}
		
		QString modelName = id.split("/").last();
		m_modelCombo->addItem(modelName, id);
	}
	
	loadCurrentSettings();
}

void AgentSettingsDialog::loadCurrentSettings()
{
	auto* agent = lmms::AgentManager::instance();
	
	m_apiKeyEdit->setText(agent->apiKey());
	
	QString currentModel = agent->model();
	int index = m_modelCombo->findData(currentModel);
	if (index >= 0)
	{
		m_modelCombo->setCurrentIndex(index);
	}
	
	if (agent->isConfigured())
	{
		m_statusLabel->setText(tr("API key setup"));
		m_statusLabel->setStyleSheet("color: #4a9f4a; font-size: 11px; margin-top: 4px;");
	}
	else
	{
		m_statusLabel->setText(tr("API key required"));
		m_statusLabel->setStyleSheet("color: red; font-size: 11px; margin-top: 4px;");
	}
}

void AgentSettingsDialog::onApiKeyChanged(const QString& text)
{
	if (text.isEmpty())
	{
		m_statusLabel->setText(tr("API key required"));
		m_statusLabel->setStyleSheet("color: red; font-size: 11px; margin-top: 4px;");
	}
}

void AgentSettingsDialog::onSaveClicked()
{
	QString apiKey = m_apiKeyEdit->text().trimmed();
	
	if (apiKey.isEmpty())
	{
		QMessageBox::warning(this, tr("Warning"), tr("Please enter an API key."));
		return;
	}
	
	auto* agent = lmms::AgentManager::instance();
	agent->setApiKey(apiKey);
	agent->setModel(m_modelCombo->currentData().toString());
	
	accept();
}

} // namespace lmms::gui
