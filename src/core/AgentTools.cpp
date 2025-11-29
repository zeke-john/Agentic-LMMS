/*
 * AgentTools.cpp - Tool implementations for AI Agent
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "AgentTools.h"
#include "ConfigManager.h"
#include "Engine.h"
#include "Song.h"
#include "Track.h"
#include "InstrumentTrack.h"
#include "MidiClip.h"
#include "Note.h"
#include "PatternStore.h"

#include <QDir>
#include <QDirIterator>
#include <QJsonDocument>

namespace lmms
{

AgentTools::AgentTools(QObject* parent)
	: QObject(parent)
{
	initializeTools();
}

AgentTools::~AgentTools() = default;

void AgentTools::registerTool(const QString& name,
							  const QString& description,
							  const QJsonObject& parameters,
							  ToolFunction function)
{
	ToolDefinition def;
	def.name = name;
	def.description = description;
	def.parameters = parameters;
	
	m_toolDefinitions[name] = def;
	m_toolFunctions[name] = function;
}

void AgentTools::initializeTools()
{
	// TEMPO TOOLS
	registerTool(
		"get_tempo",
		"Get the current tempo (BPM) of the project",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{}},
			{"required", QJsonArray{}}
		},
		[this](const QJsonObject& args) { return getTempo(args); }
	);
	
	registerTool(
		"set_tempo",
		"Set the tempo (BPM) of the project. Valid range is 10-999 BPM.",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{
				{"bpm", QJsonObject{
					{"type", "integer"},
					{"description", "The tempo in beats per minute (10-999)"},
					{"minimum", 10},
					{"maximum", 999}
				}}
			}},
			{"required", QJsonArray{"bpm"}}
		},
		[this](const QJsonObject& args) { return setTempo(args); }
	);
	
	// TRACK TOOLS
	registerTool(
		"list_tracks",
		"List all tracks in the current project with their type, name, and status",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{}},
			{"required", QJsonArray{}}
		},
		[this](const QJsonObject& args) { return listTracks(args); }
	);
	
	registerTool(
		"add_instrument_track",
		"Add a new instrument track to the project",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{
				{"name", QJsonObject{
					{"type", "string"},
					{"description", "Name for the new track (optional)"}
				}},
				{"instrument", QJsonObject{
					{"type", "string"},
					{"description", "Instrument plugin to load (e.g., 'tripleoscillator', 'sf2player'). Optional."}
				}}
			}},
			{"required", QJsonArray{}}
		},
		[this](const QJsonObject& args) { return addInstrumentTrack(args); }
	);
	
	registerTool(
		"add_sample_track",
		"Add a new sample track to the project for audio samples",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{
				{"name", QJsonObject{
					{"type", "string"},
					{"description", "Name for the new track (optional)"}
				}}
			}},
			{"required", QJsonArray{}}
		},
		[this](const QJsonObject& args) { return addSampleTrack(args); }
	);
	
	registerTool(
		"set_track_name",
		"Set the name of a track",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{
				{"track_index", QJsonObject{
					{"type", "integer"},
					{"description", "Index of the track (0-based)"}
				}},
				{"name", QJsonObject{
					{"type", "string"},
					{"description", "New name for the track"}
				}}
			}},
			{"required", QJsonArray{"track_index", "name"}}
		},
		[this](const QJsonObject& args) { return setTrackName(args); }
	);
	
	registerTool(
		"set_track_muted",
		"Mute or unmute a track",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{
				{"track_index", QJsonObject{
					{"type", "integer"},
					{"description", "Index of the track (0-based)"}
				}},
				{"muted", QJsonObject{
					{"type", "boolean"},
					{"description", "True to mute, false to unmute"}
				}}
			}},
			{"required", QJsonArray{"track_index", "muted"}}
		},
		[this](const QJsonObject& args) { return setTrackMuted(args); }
	);
	
	// SAMPLE TOOLS
	registerTool(
		"list_samples",
		"List available audio samples, optionally filtered by category or search term",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{
				{"category", QJsonObject{
					{"type", "string"},
					{"description", "Category to filter by (e.g., 'drums', 'bass', 'percussion')"}
				}},
				{"search", QJsonObject{
					{"type", "string"},
					{"description", "Search term to filter sample names"}
				}},
				{"limit", QJsonObject{
					{"type", "integer"},
					{"description", "Maximum number of samples to return (default 20)"}
				}}
			}},
			{"required", QJsonArray{}}
		},
		[this](const QJsonObject& args) { return listSamples(args); }
	);
	
	registerTool(
		"get_sample_categories",
		"Get a list of available sample categories/folders",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{}},
			{"required", QJsonArray{}}
		},
		[this](const QJsonObject& args) { return getSampleCategories(args); }
	);
	
	// NOTE/PATTERN TOOLS
	registerTool(
		"add_notes_to_track",
		"Add MIDI notes to an instrument track. Creates a clip if needed.",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{
				{"track_index", QJsonObject{
					{"type", "integer"},
					{"description", "Index of the instrument track (0-based)"}
				}},
				{"notes", QJsonObject{
					{"type", "array"},
					{"description", "Array of notes to add"},
					{"items", QJsonObject{
						{"type", "object"},
						{"properties", QJsonObject{
							{"key", QJsonObject{
								{"type", "integer"},
								{"description", "MIDI key number (0-127, where 60 is middle C)"}
							}},
							{"position", QJsonObject{
								{"type", "integer"},
								{"description", "Position in ticks from start of clip (48 ticks = 1 beat at default)"}
							}},
							{"length", QJsonObject{
								{"type", "integer"},
								{"description", "Note length in ticks (48 = quarter note, 24 = eighth note, etc.)"}
							}},
							{"volume", QJsonObject{
								{"type", "integer"},
								{"description", "Note volume (0-100, default 100)"}
							}}
						}},
						{"required", QJsonArray{"key", "position", "length"}}
					}}
				}},
				{"clip_position", QJsonObject{
					{"type", "integer"},
					{"description", "Position of the clip in ticks (default 0)"}
				}}
			}},
			{"required", QJsonArray{"track_index", "notes"}}
		},
		[this](const QJsonObject& args) { return addNotesToTrack(args); }
	);
	
	registerTool(
		"get_track_notes",
		"Get all notes from a track's clips",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{
				{"track_index", QJsonObject{
					{"type", "integer"},
					{"description", "Index of the track (0-based)"}
				}}
			}},
			{"required", QJsonArray{"track_index"}}
		},
		[this](const QJsonObject& args) { return getTrackNotes(args); }
	);
	
	registerTool(
		"clear_track_notes",
		"Clear all notes from a track",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{
				{"track_index", QJsonObject{
					{"type", "integer"},
					{"description", "Index of the track (0-based)"}
				}}
			}},
			{"required", QJsonArray{"track_index"}}
		},
		[this](const QJsonObject& args) { return clearTrackNotes(args); }
	);
	
	// PROJECT TOOLS
	registerTool(
		"get_project_info",
		"Get information about the current project",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{}},
			{"required", QJsonArray{}}
		},
		[this](const QJsonObject& args) { return getProjectInfo(args); }
	);
	
	registerTool(
		"play_project",
		"Start playing the project",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{}},
			{"required", QJsonArray{}}
		},
		[this](const QJsonObject& args) { return playProject(args); }
	);
	
	registerTool(
		"stop_project",
		"Stop playing the project",
		QJsonObject{
			{"type", "object"},
			{"properties", QJsonObject{}},
			{"required", QJsonArray{}}
		},
		[this](const QJsonObject& args) { return stopProject(args); }
	);
}

QJsonArray AgentTools::getToolDefinitions() const
{
	QJsonArray tools;
	
	for (const auto& [name, def] : m_toolDefinitions)
	{
		QJsonObject tool;
		tool["type"] = "function";
		
		QJsonObject function;
		function["name"] = def.name;
		function["description"] = def.description;
		function["parameters"] = def.parameters;
		
		tool["function"] = function;
		tools.append(tool);
	}
	
	return tools;
}

bool AgentTools::hasTool(const QString& name) const
{
	return m_toolFunctions.find(name) != m_toolFunctions.end();
}

ToolResult AgentTools::executeTool(const QString& name, const QJsonObject& args)
{
	auto it = m_toolFunctions.find(name);
	if (it == m_toolFunctions.end())
	{
		return {false, "", QString("Unknown tool: %1").arg(name)};
	}
	
	try
	{
		return it->second(args);
	}
	catch (const std::exception& e)
	{
		return {false, "", QString("Tool execution error: %1").arg(e.what())};
	}
}

// TEMPO TOOL IMPLEMENTATIONS
ToolResult AgentTools::getTempo(const QJsonObject& args)
{
	Q_UNUSED(args)
	
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	int bpm = song->getTempo();
	
	QJsonObject result;
	result["bpm"] = bpm;
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

ToolResult AgentTools::setTempo(const QJsonObject& args)
{
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	if (!args.contains("bpm"))
	{
		return {false, "", "Missing required parameter: bpm"};
	}
	
	int bpm = args["bpm"].toInt();
	if (bpm < 10 || bpm > 999)
	{
		return {false, "", QString("BPM must be between 10 and 999, got %1").arg(bpm)};
	}
	
	song->setTempo(bpm);
	
	QJsonObject result;
	result["success"] = true;
	result["bpm"] = bpm;
	result["message"] = QString("Tempo set to %1 BPM").arg(bpm);
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}


// TRACK TOOL IMPLEMENTATIONS
ToolResult AgentTools::listTracks(const QJsonObject& args)
{
	Q_UNUSED(args)
	
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	QJsonArray tracksArray;
	const auto& tracks = song->tracks();
	
	for (size_t i = 0; i < tracks.size(); ++i)
	{
		Track* track = tracks[i];
		QJsonObject trackObj;
		trackObj["index"] = static_cast<int>(i);
		trackObj["name"] = track->name();
		trackObj["muted"] = track->isMuted();
		trackObj["solo"] = track->isSolo();
		
		QString typeStr;
		switch (track->type())
		{
			case Track::Type::Instrument: typeStr = "instrument"; break;
			case Track::Type::Pattern: typeStr = "pattern"; break;
			case Track::Type::Sample: typeStr = "sample"; break;
			case Track::Type::Automation: typeStr = "automation"; break;
			default: typeStr = "other"; break;
		}
		trackObj["type"] = typeStr;
		
		// Add instrument name if it's an instrument track
		if (track->type() == Track::Type::Instrument)
		{
			auto* instTrack = dynamic_cast<InstrumentTrack*>(track);
			if (instTrack)
			{
				trackObj["instrument"] = instTrack->instrumentName();
			}
		}
		
		tracksArray.append(trackObj);
	}
	
	QJsonObject result;
	result["tracks"] = tracksArray;
	result["count"] = static_cast<int>(tracks.size());
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

ToolResult AgentTools::addInstrumentTrack(const QJsonObject& args)
{
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	Track* track = Track::create(Track::Type::Instrument, song);
	if (!track)
	{
		return {false, "", "Failed to create instrument track"};
	}
	
	// Set name if provided
	if (args.contains("name") && !args["name"].toString().isEmpty())
	{
		track->setName(args["name"].toString());
	}
	
	// Load instrument if specified
	if (args.contains("instrument") && !args["instrument"].toString().isEmpty())
	{
		auto* instTrack = dynamic_cast<InstrumentTrack*>(track);
		if (instTrack)
		{
			instTrack->loadInstrument(args["instrument"].toString());
		}
	}
	
	QJsonObject result;
	result["success"] = true;
	result["track_index"] = static_cast<int>(song->tracks().size() - 1);
	result["name"] = track->name();
	result["message"] = QString("Created instrument track: %1").arg(track->name());
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

ToolResult AgentTools::addSampleTrack(const QJsonObject& args)
{
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	Track* track = Track::create(Track::Type::Sample, song);
	if (!track)
	{
		return {false, "", "Failed to create sample track"};
	}
	
	// Set name if provided
	if (args.contains("name") && !args["name"].toString().isEmpty())
	{
		track->setName(args["name"].toString());
	}
	
	QJsonObject result;
	result["success"] = true;
	result["track_index"] = static_cast<int>(song->tracks().size() - 1);
	result["name"] = track->name();
	result["message"] = QString("Created sample track: %1").arg(track->name());
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

ToolResult AgentTools::removeTrack(const QJsonObject& args)
{
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	if (!args.contains("track_index"))
	{
		return {false, "", "Missing required parameter: track_index"};
	}
	
	int index = args["track_index"].toInt();
	const auto& tracks = song->tracks();
	
	if (index < 0 || index >= static_cast<int>(tracks.size()))
	{
		return {false, "", QString("Invalid track index: %1").arg(index)};
	}
	
	Track* track = tracks[index];
	QString trackName = track->name();
	
	delete track;
	
	QJsonObject result;
	result["success"] = true;
	result["message"] = QString("Removed track: %1").arg(trackName);
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

ToolResult AgentTools::setTrackName(const QJsonObject& args)
{
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	if (!args.contains("track_index") || !args.contains("name"))
	{
		return {false, "", "Missing required parameters: track_index, name"};
	}
	
	int index = args["track_index"].toInt();
	QString name = args["name"].toString();
	const auto& tracks = song->tracks();
	
	if (index < 0 || index >= static_cast<int>(tracks.size()))
	{
		return {false, "", QString("Invalid track index: %1").arg(index)};
	}
	
	Track* track = tracks[index];
	QString oldName = track->name();
	track->setName(name);
	
	QJsonObject result;
	result["success"] = true;
	result["old_name"] = oldName;
	result["new_name"] = name;
	result["message"] = QString("Renamed track from '%1' to '%2'").arg(oldName, name);
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

ToolResult AgentTools::setTrackMuted(const QJsonObject& args)
{
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	if (!args.contains("track_index") || !args.contains("muted"))
	{
		return {false, "", "Missing required parameters: track_index, muted"};
	}
	
	int index = args["track_index"].toInt();
	bool muted = args["muted"].toBool();
	const auto& tracks = song->tracks();
	
	if (index < 0 || index >= static_cast<int>(tracks.size()))
	{
		return {false, "", QString("Invalid track index: %1").arg(index)};
	}
	
	Track* track = tracks[index];
	track->setMuted(muted);
	
	QJsonObject result;
	result["success"] = true;
	result["track_index"] = index;
	result["muted"] = muted;
	result["message"] = QString("Track '%1' is now %2").arg(track->name(), muted ? "muted" : "unmuted");
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}


// SAMPLE TOOL IMPLEMENTATIONS
ToolResult AgentTools::listSamples(const QJsonObject& args)
{
	QString category = args["category"].toString();
	QString search = args["search"].toString().toLower();
	int limit = args.contains("limit") ? args["limit"].toInt() : 20;
	
	// get samples dirs
	QStringList sampleDirs;
	sampleDirs << ConfigManager::inst()->factorySamplesDir();
	sampleDirs << ConfigManager::inst()->userSamplesDir();
	
	QJsonArray samplesArray;
	int count = 0;
	
	for (const QString& baseDir : sampleDirs)
	{
		QString searchDir = baseDir;
		if (!category.isEmpty())
		{
			searchDir = baseDir + category + "/";
		}
		
		QDirIterator it(searchDir, 
			QStringList() << "*.wav" << "*.ogg" << "*.mp3" << "*.flac" << "*.ds",
			QDir::Files, 
			QDirIterator::Subdirectories);
		
		while (it.hasNext() && count < limit)
		{
			QString filePath = it.next();
			QString fileName = it.fileName();
			
			if (!search.isEmpty() && !fileName.toLower().contains(search))
			{
				continue;
			}
			
			QJsonObject sample;
			sample["name"] = fileName;
			sample["path"] = filePath;
			

			QString relativePath = filePath;
			relativePath.remove(baseDir);
			int slashIndex = relativePath.lastIndexOf('/');
			if (slashIndex > 0)
			{
				sample["category"] = relativePath.left(slashIndex);
			}
			
			samplesArray.append(sample);
			count++;
		}
	}
	
	QJsonObject result;
	result["samples"] = samplesArray;
	result["count"] = count;
	if (count >= limit)
	{
		result["note"] = QString("Results limited to %1. Use filters to narrow down.").arg(limit);
	}
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

ToolResult AgentTools::getSampleCategories(const QJsonObject& args)
{
	Q_UNUSED(args)
	
	QJsonArray categoriesArray;
	QStringList seenCategories;
	
	QStringList sampleDirs;
	sampleDirs << ConfigManager::inst()->factorySamplesDir();
	sampleDirs << ConfigManager::inst()->userSamplesDir();
	
	for (const QString& baseDir : sampleDirs)
	{
		QDir dir(baseDir);
		if (!dir.exists()) continue;
		
		QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
		for (const QString& subdir : subdirs)
		{
			if (!seenCategories.contains(subdir))
			{
				seenCategories << subdir;
				QDir catDir(baseDir + subdir);
				int fileCount = catDir.entryList(
					QStringList() << "*.wav" << "*.ogg" << "*.mp3" << "*.flac" << "*.ds",
					QDir::Files
				).count();
				
				QJsonObject cat;
				cat["name"] = subdir;
				cat["path"] = baseDir + subdir;
				cat["file_count"] = fileCount;
				categoriesArray.append(cat);
			}
		}
	}
	
	QJsonObject result;
	result["categories"] = categoriesArray;
	result["count"] = categoriesArray.size();
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

// NOTE/PATTERN TOOL IMPLEMENTATIONS
ToolResult AgentTools::addNotesToTrack(const QJsonObject& args)
{
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	if (!args.contains("track_index") || !args.contains("notes"))
	{
		return {false, "", "Missing required parameters: track_index, notes"};
	}
	
	int trackIndex = args["track_index"].toInt();
	QJsonArray notesArray = args["notes"].toArray();
	int clipPosition = args.contains("clip_position") ? args["clip_position"].toInt() : 0;
	
	const auto& tracks = song->tracks();
	if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size()))
	{
		return {false, "", QString("Invalid track index: %1").arg(trackIndex)};
	}
	
	Track* track = tracks[trackIndex];
	if (track->type() != Track::Type::Instrument)
	{
		return {false, "", "Track is not an instrument track"};
	}
	
	auto* instTrack = dynamic_cast<InstrumentTrack*>(track);
	if (!instTrack)
	{
		return {false, "", "Failed to cast to instrument track"};
	}
	
	// get or create a clip at the specified position
	MidiClip* clip = nullptr;
	TimePos clipPos(clipPosition);
	
	// check for existing clip at position
	for (auto* existingClip : instTrack->getClips())
	{
		if (existingClip->startPosition() == clipPos)
		{
			clip = dynamic_cast<MidiClip*>(existingClip);
			break;
		}
	}
	
	if (!clip)
	{
		auto* newClip = instTrack->createClip(clipPos);
		clip = dynamic_cast<MidiClip*>(newClip);
	}
	
	if (!clip)
	{
		return {false, "", "Failed to get or create MIDI clip"};
	}
	
	int notesAdded = 0;
	for (const QJsonValue& noteVal : notesArray)
	{
		QJsonObject noteObj = noteVal.toObject();
		
		int key = noteObj["key"].toInt();
		int position = noteObj["position"].toInt();
		int length = noteObj["length"].toInt();
		int volume = noteObj.contains("volume") ? noteObj["volume"].toInt() : 100;
		
		// validate key (MIDI range)
		if (key < 0 || key > 127)
		{
			continue;
		}
		
		// Create and add note
		Note note(TimePos(length), TimePos(position), key, 
				  static_cast<volume_t>(volume), DefaultPanning);
		clip->addNote(note, false);
		notesAdded++;
	}
	
	clip->updateLength();
	
	QJsonObject result;
	result["success"] = true;
	result["notes_added"] = notesAdded;
	result["track"] = instTrack->name();
	result["message"] = QString("Added %1 notes to track '%2'").arg(notesAdded).arg(instTrack->name());
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

ToolResult AgentTools::getTrackNotes(const QJsonObject& args)
{
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	if (!args.contains("track_index"))
	{
		return {false, "", "Missing required parameter: track_index"};
	}
	
	int trackIndex = args["track_index"].toInt();
	const auto& tracks = song->tracks();
	
	if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size()))
	{
		return {false, "", QString("Invalid track index: %1").arg(trackIndex)};
	}
	
	Track* track = tracks[trackIndex];
	if (track->type() != Track::Type::Instrument)
	{
		return {false, "", "Track is not an instrument track"};
	}
	
	auto* instTrack = dynamic_cast<InstrumentTrack*>(track);
	if (!instTrack)
	{
		return {false, "", "Failed to cast to instrument track"};
	}
	
	QJsonArray clipsArray;
	for (auto* clip : instTrack->getClips())
	{
		auto* midiClip = dynamic_cast<MidiClip*>(clip);
		if (!midiClip) continue;
		
		QJsonObject clipObj;
		clipObj["position"] = static_cast<int>(midiClip->startPosition().getTicks());
		clipObj["length"] = static_cast<int>(midiClip->length().getTicks());
		
		QJsonArray notesArray;
		for (const auto* note : midiClip->notes())
		{
			QJsonObject noteObj;
			noteObj["key"] = note->key();
			noteObj["position"] = static_cast<int>(note->pos().getTicks());
			noteObj["length"] = static_cast<int>(note->length().getTicks());
			noteObj["volume"] = static_cast<int>(note->getVolume());
			notesArray.append(noteObj);
		}
		
		clipObj["notes"] = notesArray;
		clipObj["note_count"] = notesArray.size();
		clipsArray.append(clipObj);
	}
	
	QJsonObject result;
	result["track"] = instTrack->name();
	result["clips"] = clipsArray;
	result["clip_count"] = clipsArray.size();
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

ToolResult AgentTools::clearTrackNotes(const QJsonObject& args)
{
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	if (!args.contains("track_index"))
	{
		return {false, "", "Missing required parameter: track_index"};
	}
	
	int trackIndex = args["track_index"].toInt();
	const auto& tracks = song->tracks();
	
	if (trackIndex < 0 || trackIndex >= static_cast<int>(tracks.size()))
	{
		return {false, "", QString("Invalid track index: %1").arg(trackIndex)};
	}
	
	Track* track = tracks[trackIndex];
	if (track->type() != Track::Type::Instrument)
	{
		return {false, "", "Track is not an instrument track"};
	}
	
	auto* instTrack = dynamic_cast<InstrumentTrack*>(track);
	if (!instTrack)
	{
		return {false, "", "Failed to cast to instrument track"};
	}
	
	int notesCleared = 0;
	for (auto* clip : instTrack->getClips())
	{
		auto* midiClip = dynamic_cast<MidiClip*>(clip);
		if (!midiClip) continue;
		
		notesCleared += midiClip->notes().size();
		midiClip->clearNotes();
	}
	
	QJsonObject result;
	result["success"] = true;
	result["notes_cleared"] = notesCleared;
	result["track"] = instTrack->name();
	result["message"] = QString("Cleared %1 notes from track '%2'").arg(notesCleared).arg(instTrack->name());
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}


// PROJECT TOOL IMPLEMENTATIONS
ToolResult AgentTools::getProjectInfo(const QJsonObject& args)
{
	Q_UNUSED(args)
	
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	QJsonObject result;
	result["tempo"] = song->getTempo();
	result["master_volume"] = song->masterVolume();
	result["master_pitch"] = song->masterPitch();
	result["is_playing"] = song->isPlaying();
	result["is_paused"] = song->isPaused();
	result["track_count"] = static_cast<int>(song->tracks().size());
	result["length_bars"] = static_cast<int>(song->length());
	
	QJsonObject timeSig;
	timeSig["numerator"] = song->getTimeSigModel().getNumerator();
	timeSig["denominator"] = song->getTimeSigModel().getDenominator();
	result["time_signature"] = timeSig;
	
	QString fileName = song->projectFileName();
	if (!fileName.isEmpty())
	{
		result["file_name"] = fileName;
	}
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

ToolResult AgentTools::playProject(const QJsonObject& args)
{
	Q_UNUSED(args)
	
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	if (song->isPlaying())
	{
		return {true, R"({"status": "already_playing", "message": "Project is already playing"})", ""};
	}
	
	song->playSong();
	
	QJsonObject result;
	result["status"] = "playing";
	result["message"] = "Project playback started";
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

ToolResult AgentTools::stopProject(const QJsonObject& args)
{
	Q_UNUSED(args)
	
	Song* song = Engine::getSong();
	if (!song)
	{
		return {false, "", "No project loaded"};
	}
	
	song->stop();
	
	QJsonObject result;
	result["status"] = "stopped";
	result["message"] = "Project playback stopped";
	
	return {true, QJsonDocument(result).toJson(QJsonDocument::Compact), ""};
}

} // namespace lmms

