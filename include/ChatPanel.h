/*
 * ChatPanel.h - a floating chat panel for da AI agent
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#ifndef LMMS_GUI_CHAT_PANEL_H
#define LMMS_GUI_CHAT_PANEL_H

#include <QWidget>
#include <QScrollArea>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QDomDocument>
#include <QDomElement>
#include <QJsonObject>

namespace lmms::gui
{

class CollapsibleSection;

class ChatPanel : public QWidget
{
	Q_OBJECT
public:
	ChatPanel();
	~ChatPanel() override = default;

	void saveSettings(QDomDocument& doc, QDomElement& element);
	void loadSettings(const QDomElement& element);

protected:
	void closeEvent(QCloseEvent* ce) override;
	bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
	void onSendMessage();
	void onClearHistory();
	void onSettingsClicked();
	void onResponseReceived(const QString& response);
	void onStreamingChunkReceived(const QString& chunk);
	void onThinkingChunkReceived(const QString& chunk);
	void onStreamingStarted();
	void onStreamingFinished();
	void onToolCallStarted(const QString& toolName, const QJsonObject& args);
	void onToolCallCompleted(const QString& toolName, const QString& result);
	void onErrorOccurred(const QString& error);
	void onProcessingStarted();
	void onProcessingFinished();
	void onScrollValueChanged(int value);

private:
	void repositionClearButton();
	void repositionSettingsButton();
	void showApiKeyDialog();
	void updateButtonStates();
	void scrollToBottom();
	
	QLabel* createUserMessageWidget(const QString& message);
	QLabel* createAssistantMessageWidget(const QString& message);
	QFrame* createToolCallWidget(const QString& toolName, const QString& result);
	QLabel* createErrorWidget(const QString& error);
	CollapsibleSection* createThinkingWidget(const QString& content);
	void addMessageWidget(QWidget* widget);
	void clearMessages();
	
	void updateCurrentStreamingWidgets();
	void finalizeStreamingWidgets();

	QWidget* m_chatContainer;
	QScrollArea* m_scrollArea;
	QWidget* m_messagesContainer;
	QVBoxLayout* m_messagesLayout;
	QLabel* m_emptyHintLabel;
	QLineEdit* m_inputField;
	QPushButton* m_sendButton;
	QPushButton* m_clearButton;
	QPushButton* m_settingsButton;
	
	bool m_isFirstMessage;
	bool m_isProcessing;
	bool m_isStreaming;
	bool m_streamingOutputComplete;
	
	QString m_currentStreamContent;
	QString m_currentThinkingContent;
	bool m_hasThinkingContent;

	CollapsibleSection* m_currentThinkingWidget;
	QLabel* m_currentContentWidget;
	

	CollapsibleSection* m_currentToolCallWidget;

	bool m_autoScrollEnabled;
	int m_lastScrollValue;
	bool m_isProgrammaticScroll;
};

} // namespace lmms::gui

#endif // LMMS_GUI_CHAT_PANEL_H
