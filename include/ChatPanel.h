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
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDomDocument>
#include <QDomElement>

namespace lmms::gui
{

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

private slots:
	void onSendMessage();

private:
	QTextEdit* m_chatHistory;
	QLineEdit* m_inputField;
	QPushButton* m_sendButton;
};

} // namespace lmms::gui

#endif // LMMS_GUI_CHAT_PANEL_H

