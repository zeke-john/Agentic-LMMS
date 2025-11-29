/*
 * AgentManager.cpp - AI Agent manager for LMMS with OpenRouter integration
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "AgentManager.h"
#include "AgentTools.h"
#include "ConfigManager.h"
#include "Engine.h"
#include "Song.h"

#include <QJsonDocument>
#include <QNetworkRequest>
#include <QUrl>

namespace lmms
{

static const QString OPENROUTER_API_URL = "https://openrouter.ai/api/v1/chat/completions";
AgentManager* AgentManager::s_instance = nullptr;

AgentManager* AgentManager::instance()
{
	if (!s_instance)
	{
		s_instance = new AgentManager();
	}
	return s_instance;
}

AgentManager::AgentManager()
	: QObject()
	, m_networkManager(new QNetworkAccessManager(this))
	, m_model("anthropic/claude-4-5-sonnet")
	, m_isProcessing(false)
	, m_tools(std::make_unique<AgentTools>(this))
	, m_currentToolCallIndex(0)
	, m_currentReply(nullptr)
	, m_isStreaming(false)
{
	m_apiKey = ConfigManager::inst()->value("agent", "apikey");

	// default model, need to try gemini 3
	m_model = ConfigManager::inst()->value("agent", "model", "anthropic/claude-4-5-sonnet");
}

AgentManager::~AgentManager()
{
	s_instance = nullptr;
}

void AgentManager::setApiKey(const QString& apiKey)
{
	m_apiKey = apiKey;
	ConfigManager::inst()->setValue("agent", "apikey", apiKey);
}

QString AgentManager::apiKey() const
{
	return m_apiKey;
}

void AgentManager::setModel(const QString& model)
{
	m_model = model;
	ConfigManager::inst()->setValue("agent", "model", model);
}

QString AgentManager::model() const
{
	return m_model;
}

bool AgentManager::isConfigured() const
{
	return !m_apiKey.isEmpty();
}

void AgentManager::sendMessage(const QString& userMessage)
{
	if (!isConfigured())
	{
		emit errorOccurred(tr("api key not set up... please set your openrouter api key."));
		return;
	}
	
	if (m_isProcessing)
	{
		emit errorOccurred(tr("already processing a request... please wait."));
		return;
	}
	

	ChatMessage userMsg;
	userMsg.role = "user";
	userMsg.content = userMessage;
	m_conversationHistory.push_back(userMsg);
	
	m_isProcessing = true;
	emit processingStarted();
	
	sendApiRequest();
}

void AgentManager::clearHistory()
{
	cancelCurrentRequest();
	
	m_conversationHistory.clear();
	m_pendingToolCalls.clear();
	m_currentToolCallIndex = 0;
}

void AgentManager::cancelCurrentRequest()
{
	if (m_currentReply)
	{
		disconnect(m_currentReply, nullptr, this, nullptr);
		m_currentReply->abort();
		m_currentReply->deleteLater();
		m_currentReply = nullptr;
	}
	
	m_isStreaming = false;
	m_streamBuffer.clear();
	m_accumulatedContent.clear();
	m_accumulatedThinking.clear();
	m_accumulatedToolCalls = QJsonArray();
	
	if (m_isProcessing)
	{
		m_isProcessing = false;
		emit processingFinished();
	}
}

QString AgentManager::systemPrompt() const
{
	// update this w/ tools in the future
	return QString(
		"You are an AI music production assistant integrated into LMMS (Linux MultiMedia Studio). "
		"You help users create, modify, and get inspiration for their music projects.\n\n"
		"You have access to tools that can:\n"
		"- Get and set the project tempo (BPM)\n"
		"- List, add, and manage tracks\n"
		"- Browse available samples (drums, percussion, etc.)\n"
		"- Add notes and patterns to tracks\n"
		"- Control playback\n\n"
		"When the user asks you to do something, use the appropriate tools to accomplish the task. "
		"Always explain what you're doing and provide helpful feedback.\n\n"
	).arg(Engine::getSong() ? Engine::getSong()->getTempo() : 140);
}

QJsonObject AgentManager::buildRequestPayload() const
{
	QJsonObject payload;
	payload["model"] = m_model;
	payload["stream"] = true;
	
	QJsonArray messages;
	
	QJsonObject systemMsg;
	systemMsg["role"] = "system";
	systemMsg["content"] = systemPrompt();
	messages.append(systemMsg);
	
	// add convo to payload
	for (const auto& msg : m_conversationHistory)
	{
		QJsonObject jsonMsg;
		jsonMsg["role"] = msg.role;
		
		if (msg.role == "tool")
		{
			jsonMsg["tool_call_id"] = msg.toolCallId;
			jsonMsg["content"] = msg.content;
		}
		else if (msg.role == "assistant" && !msg.toolCalls.isEmpty())
		{
			jsonMsg["content"] = msg.content;
			jsonMsg["tool_calls"] = msg.toolCalls;
		}
		else
		{
			jsonMsg["content"] = msg.content;
		}
		
		messages.append(jsonMsg);
	}
	
	payload["messages"] = messages;
	payload["tools"] = m_tools->getToolDefinitions();
	
	return payload;
}

void AgentManager::sendApiRequest()
{
	QUrl url(OPENROUTER_API_URL);
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
	request.setRawHeader("HTTP-Referer", "https://lmms.io");
	request.setRawHeader("X-Title", "LMMS AI Producer");
	
	// Reset streaming state
	m_streamBuffer.clear();
	m_accumulatedContent.clear();
	m_accumulatedThinking.clear();
	m_accumulatedToolCalls = QJsonArray();
	m_isStreaming = true;
	
	QJsonObject payload = buildRequestPayload();
	QJsonDocument doc(payload);
	
	m_currentReply = m_networkManager->post(request, doc.toJson());
	
	// Connect streaming signals
	connect(m_currentReply, &QNetworkReply::readyRead, 
			this, &AgentManager::onStreamingReadyRead);
	connect(m_currentReply, &QNetworkReply::finished, 
			this, &AgentManager::onStreamingFinished);
	
	emit streamingStarted();
}

void AgentManager::onNetworkReply(QNetworkReply* reply)
{
	// this is now only used for non-streaming fallback
	reply->deleteLater();
	
	if (reply->error() != QNetworkReply::NoError)
	{
		m_isProcessing = false;
		emit processingFinished();
		emit errorOccurred(tr("Network error: %1").arg(reply->errorString()));
		return;
	}
	
	QByteArray responseData = reply->readAll();
	QJsonDocument doc = QJsonDocument::fromJson(responseData);
	
	if (doc.isNull() || !doc.isObject())
	{
		m_isProcessing = false;
		emit processingFinished();
		emit errorOccurred(tr("Invalid response from API"));
		return;
	}
	
	processApiResponse(doc.object());
}

void AgentManager::onStreamingReadyRead()
{
	if (!m_currentReply) return;
	
	QByteArray data = m_currentReply->readAll();
	m_streamBuffer += QString::fromUtf8(data);
	
	// process sse format: data: {...}\n\n
	while (true)
	{
		int dataIndex = m_streamBuffer.indexOf("data: ");
		if (dataIndex == -1) break;
		
		int endIndex = m_streamBuffer.indexOf("\n", dataIndex);
		if (endIndex == -1) break; //wait for more data
		
		QString line = m_streamBuffer.mid(dataIndex + 6, endIndex - dataIndex - 6).trimmed();
		m_streamBuffer = m_streamBuffer.mid(endIndex + 1);
		
		if (line.isEmpty()) continue;
		if (line == "[DONE]") 
		{
			emit streamingFinished();
			continue;
		}
		
		QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
		if (doc.isNull() || !doc.isObject()) continue;
		
		QJsonObject chunk = doc.object();
		processStreamingChunk(chunk);
	}
}

void AgentManager::onStreamingFinished()
{
	if (!m_currentReply) return;
	
	if (m_currentReply->error() != QNetworkReply::NoError)
	{
		m_isProcessing = false;
		m_isStreaming = false;
		emit processingFinished();
		emit errorOccurred(tr("Network error: %1").arg(m_currentReply->errorString()));
		m_currentReply->deleteLater();
		m_currentReply = nullptr;
		return;
	}
	
	m_currentReply->deleteLater();
	m_currentReply = nullptr;
	m_isStreaming = false;
	
	// check if we have tool calls to process
	if (!m_accumulatedToolCalls.isEmpty())
	{
		ChatMessage assistantMsg;
		assistantMsg.role = "assistant";
		assistantMsg.content = m_accumulatedContent;
		assistantMsg.toolCalls = m_accumulatedToolCalls;
		m_conversationHistory.push_back(assistantMsg);
		
		handleToolCalls(m_accumulatedToolCalls);
	}
	else if (!m_accumulatedContent.isEmpty())
	{
		ChatMessage assistantMsg;
		assistantMsg.role = "assistant";
		assistantMsg.content = m_accumulatedContent;
		m_conversationHistory.push_back(assistantMsg);
		
		m_isProcessing = false;
		emit processingFinished();
		emit responseReceived(m_accumulatedContent);
	}
	else
	{
		m_isProcessing = false;
		emit processingFinished();
	}
}

void AgentManager::processStreamingChunk(const QJsonObject& chunk)
{
	if (chunk.contains("error"))
	{
		QJsonObject error = chunk["error"].toObject();
		QString errorMessage = error["message"].toString();
		emit errorOccurred(tr("API error: %1").arg(errorMessage));
		return;
	}
	
	QJsonArray choices = chunk["choices"].toArray();
	if (choices.isEmpty()) return;
	
	QJsonObject choice = choices[0].toObject();
	QJsonObject delta = choice["delta"].toObject();
	
	if (delta.contains("content"))
	{
		QString contentChunk = delta["content"].toString();
		if (!contentChunk.isEmpty())
		{
			m_accumulatedContent += contentChunk;
			emit streamingChunkReceived(contentChunk);
		}
	}
	
	if (delta.contains("reasoning") || delta.contains("thinking"))
	{
		QString thinkingKey = delta.contains("reasoning") ? "reasoning" : "thinking";
		QString thinkingChunk = delta[thinkingKey].toString();
		if (!thinkingChunk.isEmpty())
		{
			m_accumulatedThinking += thinkingChunk;
			emit thinkingChunkReceived(thinkingChunk);
		}
	}
	
	if (delta.contains("tool_calls"))
	{
		QJsonArray toolCallsArray = delta["tool_calls"].toArray();
		for (const QJsonValue& tcValue : toolCallsArray)
		{
			QJsonObject tc = tcValue.toObject();
			int index = tc["index"].toInt();
			while (m_accumulatedToolCalls.size() <= index)
			{
				QJsonObject emptyTc;
				emptyTc["id"] = "";
				QJsonObject emptyFunc;
				emptyFunc["name"] = "";
				emptyFunc["arguments"] = "";
				emptyTc["function"] = emptyFunc;
				m_accumulatedToolCalls.append(emptyTc);
			}
			
			QJsonObject existingTc = m_accumulatedToolCalls[index].toObject();
			
			if (tc.contains("id") && !tc["id"].toString().isEmpty())
			{
				existingTc["id"] = tc["id"].toString();
			}
			
			if (tc.contains("function"))
			{
				QJsonObject funcDelta = tc["function"].toObject();
				QJsonObject existingFunc = existingTc["function"].toObject();
				
				if (funcDelta.contains("name") && !funcDelta["name"].toString().isEmpty())
				{
					existingFunc["name"] = funcDelta["name"].toString();
				}
				
				if (funcDelta.contains("arguments"))
				{
					QString existingArgs = existingFunc["arguments"].toString();
					existingArgs += funcDelta["arguments"].toString();
					existingFunc["arguments"] = existingArgs;
				}
				
				existingTc["function"] = existingFunc;
			}
			
			m_accumulatedToolCalls[index] = existingTc;
		}
	}
}

void AgentManager::processApiResponse(const QJsonObject& response)
{
	if (response.contains("error"))
	{
		QJsonObject error = response["error"].toObject();
		QString errorMessage = error["message"].toString();
		m_isProcessing = false;
		emit processingFinished();
		emit errorOccurred(tr("API error: %1").arg(errorMessage));
		return;
	}
	
	QJsonArray choices = response["choices"].toArray();
	if (choices.isEmpty())
	{
		m_isProcessing = false;
		emit processingFinished();
		emit errorOccurred(tr("No response from model"));
		return;
	}
	
	QJsonObject choice = choices[0].toObject();
	QJsonObject message = choice["message"].toObject();
	QString finishReason = choice["finish_reason"].toString();
	
	// checkin for tool calls and running them
	if (message.contains("tool_calls"))
	{
		QJsonArray toolCalls = message["tool_calls"].toArray();
		
		ChatMessage assistantMsg;
		assistantMsg.role = "assistant";
		assistantMsg.content = message["content"].toString();
		assistantMsg.toolCalls = toolCalls;
		m_conversationHistory.push_back(assistantMsg);
		
		handleToolCalls(toolCalls);
	}
	else
	{
		QString content = message["content"].toString();
		
		ChatMessage assistantMsg;
		assistantMsg.role = "assistant";
		assistantMsg.content = content;
		m_conversationHistory.push_back(assistantMsg);
		
		m_isProcessing = false;
		emit processingFinished();
		emit responseReceived(content);
	}
}

void AgentManager::handleToolCalls(const QJsonArray& toolCalls)
{
	m_pendingToolCalls.clear();
	m_currentToolCallIndex = 0;

	for (const QJsonValue& tcValue : toolCalls)
	{
		QJsonObject tc = tcValue.toObject();
		ToolCall toolCall;
		toolCall.id = tc["id"].toString();
		
		QJsonObject function = tc["function"].toObject();
		toolCall.name = function["name"].toString();
		
		QString argsString = function["arguments"].toString();
		QJsonDocument argsDoc = QJsonDocument::fromJson(argsString.toUtf8());
		toolCall.arguments = argsDoc.object();
		
		m_pendingToolCalls.push_back(toolCall);
	}
	
	if (!m_pendingToolCalls.empty())
	{
		executeToolCall(m_pendingToolCalls[0]);
	}
}

void AgentManager::executeToolCall(const ToolCall& toolCall)
{
	emit toolCallStarted(toolCall.name, toolCall.arguments);
	ToolResult result = m_tools->executeTool(toolCall.name, toolCall.arguments);
	
	// add tool result to convo
	ChatMessage toolMsg;
	toolMsg.role = "tool";
	toolMsg.toolCallId = toolCall.id;
	toolMsg.name = toolCall.name;
	toolMsg.content = result.success ? result.result : result.error;
	m_conversationHistory.push_back(toolMsg);
	
	emit toolCallCompleted(toolCall.name, result.success ? result.result : result.error);

	m_currentToolCallIndex++;
	if (m_currentToolCallIndex < m_pendingToolCalls.size())
	{
		executeToolCall(m_pendingToolCalls[m_currentToolCallIndex]);
	}
	else
	{
		m_pendingToolCalls.clear();
		m_currentToolCallIndex = 0;
		sendApiRequest();
	}
}

} // namespace lmms

