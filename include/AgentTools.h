/*
 * AgentTools.h - Tool definitions and registry for AI Agent
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#ifndef LMMS_AGENT_TOOLS_H
#define LMMS_AGENT_TOOLS_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include <map>

#include "lmms_export.h"

namespace lmms
{

struct ToolDefinition
{
	QString name;
	QString description;
	QJsonObject parameters;
};

struct ToolResult
{
	bool success;
	QString result;
	QString error;
};

class LMMS_EXPORT AgentTools : public QObject
{
	Q_OBJECT

public:
	using ToolFunction = std::function<ToolResult(const QJsonObject& args)>;
	
	AgentTools(QObject* parent = nullptr);
	~AgentTools() override;
	
	// getting all tool defs for api ques
	// todo get only the relevant tools for the ques
	QJsonArray getToolDefinitions() const;
	
	
	ToolResult executeTool(const QString& name, const QJsonObject& args);
	bool hasTool(const QString& name) const;

private:
	void registerTool(const QString& name, 
					  const QString& description,
					  const QJsonObject& parameters,
					  ToolFunction function);
	
	// init all tools
	void initializeTools();
	
	// TEMPO TOOLS
	ToolResult getTempo(const QJsonObject& args);
	ToolResult setTempo(const QJsonObject& args);
	
	// TRACK TOOLS
	ToolResult listTracks(const QJsonObject& args);
	ToolResult addInstrumentTrack(const QJsonObject& args);
	ToolResult addSampleTrack(const QJsonObject& args);
	ToolResult removeTrack(const QJsonObject& args);
	ToolResult setTrackName(const QJsonObject& args);
	ToolResult setTrackMuted(const QJsonObject& args);
	
	// SAMPLE TOOLS
	ToolResult listSamples(const QJsonObject& args);
	ToolResult getSampleCategories(const QJsonObject& args);
	
	// NOTE/PATTERN TOOLS
	ToolResult addNotesToTrack(const QJsonObject& args);
	ToolResult clearTrackNotes(const QJsonObject& args);
	ToolResult getTrackNotes(const QJsonObject& args);

	// PROJECT TOOLS
	ToolResult getProjectInfo(const QJsonObject& args);
	ToolResult playProject(const QJsonObject& args);
	ToolResult stopProject(const QJsonObject& args);
	
	// add more :)

	std::map<QString, ToolDefinition> m_toolDefinitions;
	std::map<QString, ToolFunction> m_toolFunctions;
};

} // namespace lmms

#endif // LMMS_AGENT_TOOLS_H

