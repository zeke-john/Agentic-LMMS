/*
 * CollapsibleSection.h - A collapsible/expandable section widget
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#ifndef LMMS_GUI_COLLAPSIBLE_SECTION_H
#define LMMS_GUI_COLLAPSIBLE_SECTION_H

#include <QFrame>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QHBoxLayout>

namespace lmms::gui
{

class CollapsibleSection : public QWidget
{
	Q_OBJECT

public:
	explicit CollapsibleSection(const QString& title = "", int animationDuration = 100, QWidget* parent = nullptr);
	
	void setContentLayout(QLayout* contentLayout);
	void setContent(const QString& text);
	void updateContent(const QString& text);
	
	bool isExpanded() const { return m_expanded; }
	void setExpanded(bool expanded);
	
	void setTitle(const QString& title);
	void setItalic(bool italic);
	void setContentAsCode(bool asCode);

public slots:
	void toggle();

signals:
	void toggled(bool expanded);

private:
	void updateToggleButton();
	void mousePressEvent(QMouseEvent* event) override;
	
	int calculateContentHeight();
	
	QPushButton* m_toggleButton;
	QLabel* m_titleLabel;
	QPushButton* m_arrowLabel;
	QWidget* m_contentArea;
	QTextEdit* m_contentEdit;
	QVBoxLayout* m_mainLayout;
	QHBoxLayout* m_headerLayout;
	QParallelAnimationGroup* m_toggleAnimation;
	int m_animationDuration;
	int m_collapsedHeight;
	int m_maxContentHeight;
	bool m_expanded;
	bool m_italic;
	bool m_contentAsCode;
	QString m_title;
};

} // namespace lmms::gui

#endif // LMMS_GUI_COLLAPSIBLE_SECTION_H

