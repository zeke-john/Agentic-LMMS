/*
 * AgentManager.h - Agent manager for LMMS w/ OpenRouter
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#ifndef LMMS_AGENT_MANAGER_H
#define LMMS_AGENT_MANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <functional>
#include <vector>
#include <memory>

#include "lmms_export.h"

namespace lmms
{

class AgentTools;

struct ChatMessage
{
	QString role; // "user", "assistant", "system", "tool"
	QString content;
	QString toolCallId; // for tool responses
	QString name; // tool name for tool responses
	QJsonArray toolCalls; // for assistant messages with tool calls
};

struct ToolCall
{
	QString id;
	QString name;
	QJsonObject arguments;
};

class LMMS_EXPORT AgentManager : public QObject
{
	Q_OBJECT

public:
	static AgentManager* instance();
	void setApiKey(const QString& apiKey);
	QString apiKey() const;
	
	void setModel(const QString& model);
	QString model() const;
	
	void sendMessage(const QString& userMessage);
	void clearHistory();
	void cancelCurrentRequest();
	
	bool isConfigured() const;
	bool isProcessing() const { return m_isProcessing; }
	
	const std::vector<ChatMessage>& history() const { return m_conversationHistory; }

signals:
	void responseReceived(const QString& response);
	void streamingChunkReceived(const QString& chunk);
	void thinkingChunkReceived(const QString& chunk);
	void streamingStarted();
	void streamingFinished();
	void toolCallStarted(const QString& toolName, const QJsonObject& args);
	void toolCallCompleted(const QString& toolName, const QString& result);
	void errorOccurred(const QString& error);
	void processingStarted();
	void processingFinished();

private slots:
	void onNetworkReply(QNetworkReply* reply);
	void onStreamingReadyRead();
	void onStreamingFinished();

private:
	AgentManager();
	~AgentManager() override;
	AgentManager(const AgentManager&) = delete;

	AgentManager& operator=(const AgentManager&) = delete;
	
	void sendApiRequest();
	void processApiResponse(const QJsonObject& response);
	void processStreamingChunk(const QJsonObject& chunk);
	void handleToolCalls(const QJsonArray& toolCalls);
	void executeToolCall(const ToolCall& toolCall);
	
	QJsonObject buildRequestPayload() const;
	QJsonArray buildToolsArray() const;
	
	// get the system prompt
	QString systemPrompt() const;
	
	QNetworkAccessManager* m_networkManager;
	
	// config
	QString m_apiKey;
	QString m_model;
	
	// convo state
	std::vector<ChatMessage> m_conversationHistory;
	bool m_isProcessing;
	
	// tools execution
	std::unique_ptr<AgentTools> m_tools;
	
	// pending tool calls
	std::vector<ToolCall> m_pendingToolCalls;
	size_t m_currentToolCallIndex;
	
	// streaming state
	QNetworkReply* m_currentReply;
	QString m_streamBuffer;
	QString m_accumulatedContent;
	QString m_accumulatedThinking;
	QJsonArray m_accumulatedToolCalls;
	bool m_isStreaming;
	
	static AgentManager* s_instance;
};

} // namespace lmms

#endif // LMMS_AGENT_MANAGER_H

