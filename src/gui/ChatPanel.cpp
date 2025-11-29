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
#include "CollapsibleSection.h"

#include <QCloseEvent>
#include <QDomDocument>
#include <QDomElement>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QScrollBar>
#include <QTimer>

#include "embed.h"
#include "GuiApplication.h"
#include "MainWindow.h"
#include "SubWindow.h"
#include "AgentManager.h"
#include "AgentSettingsDialog.h"

namespace lmms::gui
{

ChatPanel::ChatPanel() :
	QWidget(),
	m_isFirstMessage(true),
	m_isProcessing(false),
	m_isStreaming(false),
	m_streamingOutputComplete(false),
	m_hasThinkingContent(false),
	m_currentThinkingWidget(nullptr),
	m_currentContentWidget(nullptr),
	m_currentToolCallWidget(nullptr),
	m_autoScrollEnabled(true),
	m_lastScrollValue(0),
	m_isProgrammaticScroll(false)
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
	
	m_scrollArea = new QScrollArea(m_chatContainer);
	m_scrollArea->setWidgetResizable(true);
	m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_scrollArea->setStyleSheet(
		"QScrollArea {"
		"  background-color: transparent;"
		"  border: none;"
		"}"
		"QScrollBar:vertical {"
		"  background: #1a1a1a;"
		"  width: 10px;"
		"  border: none;"
		"  margin: 0px;"
		"}"
		"QScrollBar::handle:vertical {"
		"  background: #3a3a3a;"
		"  border-radius: 5px;"
		"  min-height: 30px;"
		"  margin: 2px;"
		"}"
		"QScrollBar::handle:vertical:hover {"
		"  background: #4a4a4a;"
		"}"
		"QScrollBar::handle:vertical:pressed {"
		"  background: #5a5a5a;"
		"}"
		"QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
		"  height: 0px;"
		"  border: none;"
		"}"
		"QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
		"  background: none;"
		"}"
	);
	
	m_messagesContainer = new QWidget();
	m_messagesContainer->setStyleSheet("background-color: transparent;");
	m_messagesLayout = new QVBoxLayout(m_messagesContainer);
	m_messagesLayout->setContentsMargins(16, 16, 16, 16);
	m_messagesLayout->setSpacing(8);
	
	m_emptyHintLabel = new QLabel(tr("Create, modify, and get inspiration for your beats :)"), m_messagesContainer);
	m_emptyHintLabel->setStyleSheet(
		"QLabel {"
		"  color: #505050;"
		"  font-style: italic;"
		"  background-color: transparent;"
		"  padding: 2px 0px;"
		"}"
	);
	m_emptyHintLabel->setWordWrap(true);
	m_messagesLayout->addWidget(m_emptyHintLabel);
	m_messagesLayout->addStretch();
	
	m_scrollArea->setWidget(m_messagesContainer);
	chatContainerLayout->addWidget(m_scrollArea);
	
	QScrollBar* scrollBar = m_scrollArea->verticalScrollBar();
	connect(scrollBar, &QScrollBar::valueChanged,
			this, &ChatPanel::onScrollValueChanged);

	m_lastScrollValue = scrollBar->value();
	
	m_settingsButton = new QPushButton(m_chatContainer);
	m_settingsButton->setIcon(embed::getIconPixmap("setup_general"));
	m_settingsButton->setIconSize(QSize(12, 12));
	m_settingsButton->setFixedSize(24, 24);
	m_settingsButton->setToolTip(tr("Settings (API Key)"));
	m_settingsButton->setStyleSheet(
		"QPushButton {"
		"  background-color: rgba(40, 40, 40, 0.7);"
		"  border: none;"
		"  border-radius: 12px;"
		"}"
		"QPushButton:hover {"
		"  background-color: rgba(60, 60, 60, 0.9);"
		"}"
	);
	connect(m_settingsButton, &QPushButton::clicked, this, &ChatPanel::onSettingsClicked);
	m_settingsButton->raise();
	
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
		"QLineEdit:disabled {"
		"  background-color: #151515;"
		"  color: #606060;"
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
		"  background-color: #2a2a2a;"
		"}"
		"QPushButton:disabled {"
		"  background-color: #151515;"
		"  color: #404040;"
		"}"
	);
	connect(m_sendButton, &QPushButton::clicked, this, &ChatPanel::onSendMessage);
	inputLayout->addWidget(m_sendButton);

	mainLayout->addLayout(inputLayout);

	setLayout(mainLayout);

	QMdiSubWindow* subWin = getGUI()->mainWindow()->addWindowedWidget(this);

	Qt::WindowFlags flags = subWin->windowFlags();

	flags |= Qt::WindowMaximizeButtonHint;
	flags &= ~Qt::MSWindowsFixedSizeDialogHint;
	subWin->setWindowFlags(flags);

	subWin->setAttribute(Qt::WA_DeleteOnClose, false);

	QRect screenRect = getGUI()->mainWindow()->geometry();
	int panelWidth = 450;
	int panelHeight = screenRect.height() - 100;
	int xPos = screenRect.width() - panelWidth - 20;
	int yPos = 50;

	subWin->move(xPos, yPos);
	subWin->resize(panelWidth, panelHeight);
	subWin->setMinimumSize(300, 200);
	
	auto* agent = lmms::AgentManager::instance();
	connect(agent, &lmms::AgentManager::responseReceived, 
			this, &ChatPanel::onResponseReceived);
	connect(agent, &lmms::AgentManager::streamingChunkReceived,
			this, &ChatPanel::onStreamingChunkReceived);
	connect(agent, &lmms::AgentManager::thinkingChunkReceived,
			this, &ChatPanel::onThinkingChunkReceived);
	connect(agent, &lmms::AgentManager::streamingStarted,
			this, &ChatPanel::onStreamingStarted);
	connect(agent, &lmms::AgentManager::streamingFinished,
			this, &ChatPanel::onStreamingFinished);
	connect(agent, &lmms::AgentManager::toolCallStarted,
			this, &ChatPanel::onToolCallStarted);
	connect(agent, &lmms::AgentManager::toolCallCompleted,
			this, &ChatPanel::onToolCallCompleted);
	connect(agent, &lmms::AgentManager::errorOccurred,
			this, &ChatPanel::onErrorOccurred);
	connect(agent, &lmms::AgentManager::processingStarted,
			this, &ChatPanel::onProcessingStarted);
	connect(agent, &lmms::AgentManager::processingFinished,
			this, &ChatPanel::onProcessingFinished);
}

void ChatPanel::onSendMessage()
{
	if (m_isProcessing)
	{
		return;
	}
	
	QString message = m_inputField->text().trimmed();
	if (message.isEmpty())
	{
		return;
	}
	
	auto* agent = lmms::AgentManager::instance();
	
	if (!agent->isConfigured())
	{
		showApiKeyDialog();
		if (!agent->isConfigured())
		{
			addMessageWidget(createErrorWidget(tr("Please setup your OpenRouter API key")));
			return;
		}
	}

	m_autoScrollEnabled = true;
	
	addMessageWidget(createUserMessageWidget(message));
	m_inputField->clear();
	
	agent->sendMessage(message);
}

void ChatPanel::onClearHistory()
{
	clearMessages();
	m_isFirstMessage = true;
	m_isStreaming = false;
	m_streamingOutputComplete = false;
	m_currentStreamContent.clear();
	m_currentThinkingContent.clear();
	m_hasThinkingContent = false;
	m_currentThinkingWidget = nullptr;
	m_currentContentWidget = nullptr;
	m_autoScrollEnabled = true;
	
	lmms::AgentManager::instance()->clearHistory();
}

void ChatPanel::onSettingsClicked()
{
	showApiKeyDialog();
}

void ChatPanel::showApiKeyDialog()
{
	AgentSettingsDialog dialog(this);
	dialog.exec();
}

void ChatPanel::onResponseReceived(const QString& response)
{
	if (m_streamingOutputComplete)
	{
		m_streamingOutputComplete = false;
		return;
	}
	
	if (!m_isStreaming)
	{
		addMessageWidget(createAssistantMessageWidget(response));
	}
}

void ChatPanel::onStreamingStarted()
{
	m_isStreaming = true;
	m_streamingOutputComplete = false;
	m_currentStreamContent.clear();
	m_currentThinkingContent.clear();
	m_hasThinkingContent = false;
	m_currentThinkingWidget = nullptr;
	m_currentContentWidget = nullptr;
}

void ChatPanel::onStreamingChunkReceived(const QString& chunk)
{
	m_currentStreamContent += chunk;
	updateCurrentStreamingWidgets();
}

void ChatPanel::onThinkingChunkReceived(const QString& chunk)
{
	m_currentThinkingContent += chunk;
	m_hasThinkingContent = true;
	updateCurrentStreamingWidgets();
}

void ChatPanel::onStreamingFinished()
{
	m_isStreaming = false;
	m_streamingOutputComplete = true;
	finalizeStreamingWidgets();
}

void ChatPanel::updateCurrentStreamingWidgets()
{
	if (m_hasThinkingContent && !m_currentThinkingContent.isEmpty())
	{
		if (!m_currentThinkingWidget)
		{
			m_currentThinkingWidget = createThinkingWidget(m_currentThinkingContent);
			addMessageWidget(m_currentThinkingWidget);
		}
		else
		{
			QString displayContent = m_currentThinkingContent;
			if (displayContent.length() > 800)
			{
				displayContent = "..." + displayContent.right(800);
			}
			m_currentThinkingWidget->updateContent(displayContent);
		}
	}
	
	if (!m_currentStreamContent.isEmpty())
	{
		if (!m_currentContentWidget)
		{
			m_currentContentWidget = createAssistantMessageWidget(m_currentStreamContent);
			addMessageWidget(m_currentContentWidget);
		}
		else
		{
			m_currentContentWidget->setText(m_currentStreamContent);
		}
	}
	
	scrollToBottom();
}

void ChatPanel::finalizeStreamingWidgets()
{
	if (m_currentThinkingWidget && !m_currentThinkingContent.isEmpty())
	{
		m_currentThinkingWidget->updateContent(m_currentThinkingContent);
	}
	
	if (m_currentContentWidget && !m_currentStreamContent.isEmpty())
	{
		m_currentContentWidget->setText(m_currentStreamContent);
	}
	
	m_currentThinkingWidget = nullptr;
	m_currentContentWidget = nullptr;
	m_currentStreamContent.clear();
	m_currentThinkingContent.clear();
	m_hasThinkingContent = false;
	
	scrollToBottom();
}

void ChatPanel::onToolCallStarted(const QString& toolName, const QJsonObject& args)
{
	Q_UNUSED(args)
	
	QString displayName = toolName;
	displayName.replace("_", " ");

	QString title = QString("<b>Calling</b> %1").arg(displayName.toHtmlEscaped());
	m_currentToolCallWidget = new CollapsibleSection(title, 100);
	m_currentToolCallWidget->setItalic(false);
	m_currentToolCallWidget->setContentAsCode(true);
	m_currentToolCallWidget->setContent("");
	m_currentToolCallWidget->setExpanded(false);
	
	addMessageWidget(m_currentToolCallWidget);
}

void ChatPanel::onToolCallCompleted(const QString& toolName, const QString& result)
{
	Q_UNUSED(toolName)
	
	if (m_currentToolCallWidget)
	{
		QString displayResult = result;
		QJsonParseError parseError;
		QJsonDocument doc = QJsonDocument::fromJson(result.toUtf8(), &parseError);
		if (parseError.error == QJsonParseError::NoError && doc.isObject())
		{
			QByteArray formattedJson = doc.toJson(QJsonDocument::Indented);
			displayResult = QString::fromUtf8(formattedJson);
			displayResult = displayResult.trimmed();
		}
		
		m_currentToolCallWidget->updateContent(displayResult);
		m_currentToolCallWidget = nullptr;
	}
}

void ChatPanel::onErrorOccurred(const QString& error)
{
	addMessageWidget(createErrorWidget(error));
}

void ChatPanel::onProcessingStarted()
{
	m_isProcessing = true;
	updateButtonStates();
}

void ChatPanel::onProcessingFinished()
{
	m_isProcessing = false;
	updateButtonStates();
}

void ChatPanel::updateButtonStates()
{
	m_sendButton->setEnabled(!m_isProcessing);
	m_inputField->setEnabled(!m_isProcessing);
	
	if (m_isProcessing)
	{
		m_sendButton->setText(tr("..."));
	}
	else
	{
		m_sendButton->setText(tr("Send"));
	}
}

QLabel* ChatPanel::createUserMessageWidget(const QString& message)
{
	auto* label = new QLabel();
	label->setText(QString("<span style='color: #20C20E;'><b>&gt;</b></span> %1").arg(message.toHtmlEscaped()));
	label->setTextFormat(Qt::RichText);
	label->setWordWrap(true);
	label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	label->setStyleSheet(
		"QLabel {"
		"  color: #ffffff;"
		"  font-size: 13px;"
		"  background-color: transparent;"
		"  padding: 4px 0px;"
		"}"
	);
	m_isFirstMessage = false;
	return label;
}

QLabel* ChatPanel::createAssistantMessageWidget(const QString& message)
{
	auto* label = new QLabel(message);
	label->setTextFormat(Qt::PlainText);
	label->setWordWrap(true);
	label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	label->setStyleSheet(
		"QLabel {"
		"  color: #b0b0b0;"
		"  font-size: 13px;"
		"  background-color: transparent;"
		"  padding: 4px 0px;"
		"}"
	);
	return label;
}

QFrame* ChatPanel::createToolCallWidget(const QString& toolName, const QString& result)
{
	Q_UNUSED(toolName)
	
	auto* frame = new QFrame();
	frame->setStyleSheet(
		"QFrame {"
		"  background-color: #1a1a1a;"
		"  border-radius: 6px;"
		"  padding: 6px 10px;"
		"}"
	);
	
	auto* layout = new QVBoxLayout(frame);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	
	QString displayResult = result;
	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(result.toUtf8(), &parseError);
	if (parseError.error == QJsonParseError::NoError && doc.isObject())
	{
		QByteArray formattedJson = doc.toJson(QJsonDocument::Indented);
		displayResult = QString::fromUtf8(formattedJson);
		displayResult = displayResult.trimmed();
	}
	
	auto* resultLabel = new QLabel(displayResult);
	resultLabel->setWordWrap(true);
	resultLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	resultLabel->setStyleSheet(
		"QLabel {"
		"  color: #888888;"
		"  font-size: 10px;"
		"  font-family: 'Monaco', 'Menlo', 'Courier New', monospace;"
		"  background-color: transparent;"
		"  padding: 2px 0px;"
		"  line-height: 1.3;"
		"}"
	);
	layout->addWidget(resultLabel);
	
	return frame;
}

QLabel* ChatPanel::createErrorWidget(const QString& error)
{
	auto* label = new QLabel(error);
	label->setWordWrap(true);
	label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
	label->setStyleSheet(
		"QLabel {"
		"  color: #ff4444;"
		"  font-size: 13px;"
		"  background-color: transparent;"
		"  padding: 4px 0px;"
		"}"
	);
	return label;
}

CollapsibleSection* ChatPanel::createThinkingWidget(const QString& content)
{
	auto* section = new CollapsibleSection("Thinking", 100);
	section->setContent(content);
	section->setExpanded(true);
	return section;
}

void ChatPanel::addMessageWidget(QWidget* widget)
{
	int count = m_messagesLayout->count();
	if (m_emptyHintLabel && m_emptyHintLabel->isVisible())
	{
		m_emptyHintLabel->hide();
	}
	m_messagesLayout->insertWidget(count - 1, widget);
	scrollToBottom();
}

void ChatPanel::clearMessages()
{
	QList<QWidget*> widgetsToDelete;
	
	for (int i = 0; i < m_messagesLayout->count(); ++i)
	{
		QLayoutItem* item = m_messagesLayout->itemAt(i);
		if (item && item->widget() && item->widget() != m_emptyHintLabel)
		{
			widgetsToDelete.append(item->widget());
		}
	}
	
	for (QWidget* widget : widgetsToDelete)
	{
		widget->deleteLater();
	}
	
	if (m_emptyHintLabel)
	{
		m_emptyHintLabel->show();
	}
}

void ChatPanel::scrollToBottom()
{
	// only auto-scroll you havent scrolled up manually
	if (!m_autoScrollEnabled)
	{
		return;
	}
	
	// mark this as programmatic scroll so we dont disable auto-scroll
	m_isProgrammaticScroll = true;
	
	QTimer::singleShot(10, [this]() {
		QScrollBar* scrollBar = m_scrollArea->verticalScrollBar();
		int maxValue = scrollBar->maximum();
		scrollBar->setValue(maxValue);
		m_lastScrollValue = maxValue;

		QTimer::singleShot(50, [this]() {
			m_isProgrammaticScroll = false;
		});
	});
}

void ChatPanel::onScrollValueChanged(int value)
{
	if (m_isProgrammaticScroll)
	{
		m_lastScrollValue = value;
		return;
	}
	
	QScrollBar* scrollBar = m_scrollArea->verticalScrollBar();
	int maximum = scrollBar->maximum();
	
	bool scrolledUp = (value < m_lastScrollValue);
	
	const int threshold = 50;
	bool atBottom = (maximum - value) <= threshold;
	
	if (scrolledUp && !atBottom)
	{
		m_autoScrollEnabled = false;
	}
	else if (atBottom)
	{
		m_autoScrollEnabled = true;
	}
	
	m_lastScrollValue = value;
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
	if (parentWidget())
	{
		parentWidget()->hide();
	}
	else
	{
		hide();
	}
	ce->ignore();
}

bool ChatPanel::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == m_chatContainer && event->type() == QEvent::Resize)
	{
		repositionClearButton();
		repositionSettingsButton();
	}
	return QWidget::eventFilter(watched, event);
}

void ChatPanel::repositionClearButton()
{
	int x = m_chatContainer->width() - m_clearButton->width() - 8;
	m_clearButton->move(x, 8);
}

void ChatPanel::repositionSettingsButton()
{
	int x = m_chatContainer->width() - m_clearButton->width() - m_settingsButton->width() - 12;
	m_settingsButton->move(x, 8);
}

} // namespace lmms::gui
