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
#include <QEvent>
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

	setStyleSheet("background-color: #0d0d0d;");

	auto mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(12, 12, 12, 12);
	mainLayout->setSpacing(6);
	
	m_chatContainer = new QWidget(this);
	m_chatContainer->setStyleSheet(
		"QWidget#chatContainer {"
		"  background-color: #141414;"
		"  border: none;"
		"  border-radius: 12px;"
		"}"
	);
	m_chatContainer->setObjectName("chatContainer");
	auto chatContainerLayout = new QVBoxLayout(m_chatContainer);
	chatContainerLayout->setContentsMargins(0, 0, 0, 0);
	chatContainerLayout->setSpacing(0);
	
	m_chatHistory = new QTextEdit(m_chatContainer);
	m_chatHistory->setReadOnly(true);
	m_chatHistory->setPlaceholderText(tr("Create, modify, and get inspiration for your beats :)"));
	m_chatHistory->setStyleSheet(
		"QTextEdit {"
		"  background-color: transparent;"
		"  color: #b0b0b0;"
		"  border: none;"
		"  padding: 16px;"
		"  font-size: 13px;"
		"}"
		"QScrollBar:vertical {"
		"  background: transparent;"
		"  width: 6px;"
		"  margin: 4px 2px 4px 0px;"
		"}"
		"QScrollBar::handle:vertical {"
		"  background: #2a2a2a;"
		"  border-radius: 3px;"
		"  min-height: 20px;"
		"}"
		"QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
		"  height: 0px;"
		"}"
	);
	chatContainerLayout->addWidget(m_chatHistory);
	
	m_clearButton = new QPushButton(m_chatContainer);
	m_clearButton->setIcon(embed::getIconPixmap("clear_history"));
	m_clearButton->setIconSize(QSize(12, 12));
	m_clearButton->setFixedSize(24, 24);
	m_clearButton->setToolTip(tr("Clear History"));
	m_clearButton->setStyleSheet(
		"QPushButton {"
		"  background-color: rgba(40, 40, 40, 0.7);"
		"  border: none;"
		"  border-radius: 12px;"
		"}"
		"QPushButton:hover {"
		"  background-color: rgba(60, 60, 60, 0.9);"
		"}"
	);
	connect(m_clearButton, &QPushButton::clicked, this, &ChatPanel::onClearHistory);
	m_clearButton->raise();
	
	m_chatContainer->installEventFilter(this);
	
	mainLayout->addWidget(m_chatContainer, 1);

	auto inputLayout = new QHBoxLayout();
	inputLayout->setSpacing(6);

	m_inputField = new QLineEdit(this);
	m_inputField->setPlaceholderText(tr("Type a message..."));
	m_inputField->setStyleSheet(
		"QLineEdit {"
		"  background-color: #1a1a1a;"
		"  color: #c0c0c0;"
		"  border: none;"
		"  border-radius: 12px;"
		"  padding: 12px 16px;"
		"  font-size: 13px;"
		"}"
		"QLineEdit::placeholder {"
		"  color: #505050;"
		"}"
	);
	connect(m_inputField, &QLineEdit::returnPressed, this, &ChatPanel::onSendMessage);
	inputLayout->addWidget(m_inputField, 1);

	m_sendButton = new QPushButton(tr("Send"), this);
	m_sendButton->setStyleSheet(
		"QPushButton {"
		"  background-color: #1a1a1a;"
		"  color: #808080;"
		"  border: none;"
		"  border-radius: 12px;"
		"  padding: 13.5px 20px;"
		"  font-size: 13px;"
		"}"
		"QPushButton:hover {"
		"  color: #white;"
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
	
	QString msgFormat = "<span style='color: #20C20E;'><b>></b></span> <span style='color: #ffffff; font-style: italic;'>%1</span>";
	m_chatHistory->append(QString(msgFormat).arg(message));
	m_chatHistory->append(QString("<div style='height: 8px;'></div><span style='color: #b0b0b0;'>%1</span>").arg("blah blah blah"));
	
	isFirstMessage = false;

	m_inputField->clear();

	QScrollBar* scrollBar = m_chatHistory->verticalScrollBar();
	scrollBar->setValue(scrollBar->maximum());
}

void ChatPanel::onClearHistory()
{
	m_chatHistory->clear();
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

bool ChatPanel::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == m_chatContainer && event->type() == QEvent::Resize) {
		repositionClearButton();
	}
	return QWidget::eventFilter(watched, event);
}

void ChatPanel::repositionClearButton()
{
	int x = m_chatContainer->width() - m_clearButton->width() - 8;
	m_clearButton->move(x, 8);
}

} // namespace lmms::gui

