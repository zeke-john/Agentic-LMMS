/*
 * ChatPanel.cpp - A floating chat panel for AI assistance
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "ChatPanel.h"

#include <QCloseEvent>
#include <QDomDocument>
#include <QDomElement>
#include <QLabel>
#include <QScrollBar>

#include "embed.h"
#include "GuiApplication.h"
#include "MainWindow.h"
#include "SubWindow.h"

namespace lmms::gui
{

ChatPanel::ChatPanel() :
	QWidget()
{
	setWindowIcon(embed::getIconPixmap("text_block"));
	setWindowTitle(tr("Your AI Producer"));

	auto mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(8, 8, 8, 8);
	mainLayout->setSpacing(8);
	
	m_chatHistory = new QTextEdit(this);
	m_chatHistory->setReadOnly(true);
	m_chatHistory->setPlaceholderText(tr("Create, modify, and get inspiration for your beats here :)"));
	m_chatHistory->setStyleSheet(
		"QTextEdit {"
		"  background-color: #1a1a2e;"
		"  color: #e0e0e0;"
		"  border: 1px solid #333355;"
		"  border-radius: 4px;"
		"  padding: 8px;"
		"}"
	);
	mainLayout->addWidget(m_chatHistory, 1);

	auto inputLayout = new QHBoxLayout();
	inputLayout->setSpacing(8);

	m_inputField = new QLineEdit(this);
	m_inputField->setPlaceholderText(tr("Type a message..."));
	m_inputField->setStyleSheet(
		"QLineEdit {"
		"  background-color: #252540;"
		"  color: #e0e0e0;"
		"  border: 1px solid #333355;"
		"  border-radius: 4px;"
		"  padding: 8px;"
		"}"
	);
	connect(m_inputField, &QLineEdit::returnPressed, this, &ChatPanel::onSendMessage);
	inputLayout->addWidget(m_inputField, 1);

	m_sendButton = new QPushButton(tr("Send"), this);
	m_sendButton->setStyleSheet(
		"QPushButton {"
		"  background-color: #4a4a8a;"
		"  color: #ffffff;"
		"  border: none;"
		"  border-radius: 4px;"
		"  padding: 9px 16px;"
		"}"
		"QPushButton:hover {"
		"  background-color: #5a5a9a;"
		"  cursor: pointer;"
		"}"
	);
	connect(m_sendButton, &QPushButton::clicked, this, &ChatPanel::onSendMessage);
	inputLayout->addWidget(m_sendButton);

	mainLayout->addLayout(inputLayout);

	setLayout(mainLayout);

	QMdiSubWindow* subWin = getGUI()->mainWindow()->addWindowedWidget(this);

	Qt::WindowFlags flags = subWin->windowFlags();
	flags &= ~Qt::WindowMaximizeButtonHint;
	subWin->setWindowFlags(flags);

	subWin->setAttribute(Qt::WA_DeleteOnClose, false);

	QRect screenRect = getGUI()->mainWindow()->geometry();
	int panelWidth = 450;
	int panelHeight = screenRect.height() - 100;
	int xPos = screenRect.width() - panelWidth - 20;
	int yPos = 50;

	subWin->move(xPos, yPos);
	subWin->resize(panelWidth, panelHeight);
	subWin->setFixedWidth(panelWidth);
	subWin->setMinimumHeight(300);
}

void ChatPanel::onSendMessage()
{
	QString message = m_inputField->text().trimmed();
	if (message.isEmpty()) {
		return;
	}

	static bool isFirstMessage = true;
	
	if (!isFirstMessage) {
		m_chatHistory->append("<div style='margin-top: 10px;'></div>");
	}
	
	QString msgFormat = "<span style='color: #6a9fff;'><b>You:</b></span> %1";
	m_chatHistory->append(QString(msgFormat).arg(message));
	m_chatHistory->append(QString("%1").arg("blah blah blah"));
	
	isFirstMessage = false;

	m_inputField->clear();

	QScrollBar* scrollBar = m_chatHistory->verticalScrollBar();
	scrollBar->setValue(scrollBar->maximum());
}

void ChatPanel::saveSettings(QDomDocument& doc, QDomElement& element)
{
	Q_UNUSED(doc)
	MainWindow::saveWidgetState(this, element);
}

void ChatPanel::loadSettings(const QDomElement& element)
{
	MainWindow::restoreWidgetState(this, element);
}

void ChatPanel::closeEvent(QCloseEvent* ce)
{
	if (parentWidget()) {
		parentWidget()->hide();
	} else {
		hide();
	}
	ce->ignore();
}

} // namespace lmms::gui

