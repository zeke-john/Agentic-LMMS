/*
 * CollapsibleSection.cpp - A collapsible/expandable section widget
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "CollapsibleSection.h"

#include <QPropertyAnimation>
#include <QTransform>
#include <QFile>
#include <QDir>
#include <QIcon>
#include <QSvgRenderer>
#include <QPainter>
#include <QStandardPaths>
#include <QMouseEvent>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>

#include "embed.h"

namespace lmms::gui
{

CollapsibleSection::CollapsibleSection(const QString& title, int animationDuration, QWidget* parent)
	: QWidget(parent)
	, m_animationDuration(animationDuration)
	, m_maxContentHeight(300)
	, m_expanded(true)
	, m_italic(true)
	, m_contentAsCode(false)
{
	m_title = title.isEmpty() ? "Thinking" : title;
	if (!m_title.isEmpty())
	{
		m_title[0] = m_title[0].toUpper();
	}
	
	m_headerLayout = new QHBoxLayout();
	m_headerLayout->setContentsMargins(0, 0, 0, 0);
	m_headerLayout->setSpacing(2);
	
	m_titleLabel = new QLabel(m_title, this);
	m_titleLabel->setTextFormat(Qt::RichText);
	m_titleLabel->setStyleSheet(
		"QLabel {"
		"  background-color: transparent;"
		"  border: none;"
		"  color: #707072;"
		"  font-size: 12px;"
		"  font-style: italic;"
		"  padding: 0px;"
		"  margin: 0px;"
		"}"
	);
	m_titleLabel->setCursor(Qt::PointingHandCursor);
	
	m_toggleButton = new QPushButton(this);
	m_toggleButton->setVisible(false);
	
	m_arrowLabel = new QPushButton(this);
	m_arrowLabel->setFixedSize(12, 12);
	m_arrowLabel->setFlat(true);
	m_arrowLabel->setFocusPolicy(Qt::NoFocus);
	m_arrowLabel->setStyleSheet(
		"QPushButton {"
		"  background-color: transparent;"
		"  border: none;"
		"  padding: 2px 0px 0px 0px;"
		"  margin: 0px;"
		"}"
		"QPushButton:hover {"
		"  background-color: transparent;"
		"}"
		"QPushButton:pressed {"
		"  background-color: transparent;"
		"}"
	);
	connect(m_arrowLabel, &QPushButton::clicked, this, &CollapsibleSection::toggle);
	updateToggleButton();
	
	m_headerLayout->addWidget(m_titleLabel);
	m_headerLayout->addWidget(m_arrowLabel);
	m_headerLayout->addStretch();
	
	m_contentArea = new QWidget(this);
	m_contentArea->setStyleSheet("background-color: transparent;");
	m_contentArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_contentArea->setMaximumHeight(0);
	
	auto* contentLayout = new QVBoxLayout(m_contentArea);
	contentLayout->setContentsMargins(0, 0, 0, 0);
	contentLayout->setSpacing(0);
	
	m_contentEdit = new QTextEdit(m_contentArea);
	m_contentEdit->setReadOnly(true);
	m_contentEdit->setFrameShape(QFrame::NoFrame);
	m_contentEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_contentEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_contentEdit->setStyleSheet(
		"QTextEdit {"
		"  color: #707072;"
		"  font-size: 12px;"
		"  font-style: italic;"
		"  background-color: transparent;"
		"  border: none;"
		"  padding: 0px;"
		"  margin: 0px;"
		"}"
		"QScrollBar:vertical {"
		"  background: transparent;"
		"  width: 6px;"
		"  border: none;"
		"  margin: 0px;"
		"}"
		"QScrollBar::handle:vertical {"
		"  background: #4a4a4a;"
		"  border-radius: 3px;"
		"  min-height: 20px;"
		"}"
		"QScrollBar::handle:vertical:hover {"
		"  background: #5a5a5a;"
		"}"
		"QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
		"  height: 0px;"
		"}"
		"QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
		"  background: none;"
		"}"
	);
	contentLayout->addWidget(m_contentEdit);
	
	m_toggleAnimation = new QParallelAnimationGroup(this);
	
	auto* contentAnimation = new QPropertyAnimation(m_contentArea, "maximumHeight", this);
	contentAnimation->setDuration(m_animationDuration);
	contentAnimation->setEasingCurve(QEasingCurve::InOutQuad);
	m_toggleAnimation->addAnimation(contentAnimation);
	
	m_mainLayout = new QVBoxLayout(this);
	m_mainLayout->setSpacing(4);
	m_mainLayout->setContentsMargins(0, 0, 0, 0);
	m_mainLayout->addLayout(m_headerLayout);
	m_mainLayout->addWidget(m_contentArea);
	
	setStyleSheet("CollapsibleSection { background-color: transparent; border: none; }");
	setContentsMargins(0, 0, 0, 0);
	
	m_collapsedHeight = m_titleLabel->sizeHint().height();
	
	setExpanded(true);
}

void CollapsibleSection::mousePressEvent(QMouseEvent* event)
{
	if (event->y() <= m_collapsedHeight + 4)
	{
		toggle();
		event->accept();
	}
	else
	{
		QWidget::mousePressEvent(event);
	}
}

int CollapsibleSection::calculateContentHeight()
{
	QTextDocument* doc = m_contentEdit->document();
	doc->setTextWidth(m_contentEdit->viewport()->width());
	int docHeight = static_cast<int>(doc->size().height());
	
	QMargins margins = m_contentArea->layout()->contentsMargins();
	int totalHeight = docHeight + margins.top() + margins.bottom() + 8;
	
	return qMin(qMax(totalHeight, 30), m_maxContentHeight);
}

void CollapsibleSection::setContent(const QString& text)
{
	m_contentEdit->setPlainText(text);
	
	if (m_expanded)
	{
		int height = calculateContentHeight();
		m_contentArea->setMaximumHeight(height);
	}
}

void CollapsibleSection::updateContent(const QString& text)
{
	m_contentEdit->setPlainText(text);
	
	if (m_expanded)
	{
		int height = calculateContentHeight();
		m_contentArea->setMaximumHeight(height);
	}
}

void CollapsibleSection::setContentLayout(QLayout* contentLayout)
{
	delete m_contentArea->layout();
	m_contentArea->setLayout(contentLayout);
}

void CollapsibleSection::setTitle(const QString& title)
{
	m_title = title;
	if (!m_title.isEmpty() && !m_title.startsWith("<"))
	{
		m_title[0] = m_title[0].toUpper();
	}
	m_titleLabel->setText(m_title);
}

void CollapsibleSection::setItalic(bool italic)
{
	m_italic = italic;
	
	QString fontStyle = m_italic ? "italic" : "normal";
	m_titleLabel->setStyleSheet(
		QString(
		"QLabel {"
		"  background-color: transparent;"
		"  border: none;"
		"  color: #707072;"
		"  font-size: 12px;"
		"  font-style: %1;"
		"  padding: 0px;"
		"  margin: 0px;"
		"}"
		).arg(fontStyle)
	);
	
	if (!m_contentAsCode)
	{
		m_contentEdit->setStyleSheet(
			QString(
			"QTextEdit {"
			"  color: #707072;"
			"  font-size: 12px;"
			"  font-style: %1;"
			"  background-color: transparent;"
			"  border: none;"
			"  padding: 0px;"
			"  margin: 0px;"
			"}"
			"QScrollBar:vertical {"
			"  background: transparent;"
			"  width: 6px;"
			"  border: none;"
			"  margin: 0px;"
			"}"
			"QScrollBar::handle:vertical {"
			"  background: #4a4a4a;"
			"  border-radius: 3px;"
			"  min-height: 20px;"
			"}"
			"QScrollBar::handle:vertical:hover {"
			"  background: #5a5a5a;"
			"}"
			"QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
			"  height: 0px;"
			"}"
			"QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
			"  background: none;"
			"}"
			).arg(fontStyle)
		);
	}
}

void CollapsibleSection::setContentAsCode(bool asCode)
{
	m_contentAsCode = asCode;
	
	if (m_contentAsCode)
	{
		m_contentArea->setStyleSheet(
			"QWidget {"
			"  background-color: #1a1a1a;"
			"  border-radius: 6px;"
			"}"
		);
		
		if (m_contentArea->layout())
		{
			m_contentArea->layout()->setContentsMargins(10, 6, 10, 6);
		}
		
		m_contentEdit->setStyleSheet(
			"QTextEdit {"
			"  color: #888888;"
			"  font-size: 10px;"
			"  font-family: 'Monaco', 'Menlo', 'Courier New', monospace;"
			"  font-style: normal;"
			"  background-color: transparent;"
			"  border: none;"
			"  padding: 0px;"
			"  margin: 0px;"
			"}"
			"QScrollBar:vertical {"
			"  background: transparent;"
			"  width: 6px;"
			"  border: none;"
			"  margin: 0px;"
			"}"
			"QScrollBar::handle:vertical {"
			"  background: #4a4a4a;"
			"  border-radius: 3px;"
			"  min-height: 20px;"
			"}"
			"QScrollBar::handle:vertical:hover {"
			"  background: #5a5a5a;"
			"}"
			"QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
			"  height: 0px;"
			"}"
			"QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
			"  background: none;"
			"}"
		);
	}
	else
	{
		m_contentArea->setStyleSheet("background-color: transparent;");
		if (m_contentArea->layout())
		{
			m_contentArea->layout()->setContentsMargins(0, 0, 0, 0);
		}
		
		QString fontStyle = m_italic ? "italic" : "normal";
		m_contentEdit->setStyleSheet(
			QString(
			"QTextEdit {"
			"  color: #707072;"
			"  font-size: 12px;"
			"  font-style: %1;"
			"  background-color: transparent;"
			"  border: none;"
			"  padding: 0px;"
			"  margin: 0px;"
			"}"
			"QScrollBar:vertical {"
			"  background: transparent;"
			"  width: 6px;"
			"  border: none;"
			"  margin: 0px;"
			"}"
			"QScrollBar::handle:vertical {"
			"  background: #4a4a4a;"
			"  border-radius: 3px;"
			"  min-height: 20px;"
			"}"
			"QScrollBar::handle:vertical:hover {"
			"  background: #5a5a5a;"
			"}"
			"QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
			"  height: 0px;"
			"}"
			"QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
			"  background: none;"
			"}"
			).arg(fontStyle)
		);
	}
}

void CollapsibleSection::toggle()
{
	setExpanded(!m_expanded);
}

void CollapsibleSection::setExpanded(bool expanded)
{
	m_expanded = expanded;
	updateToggleButton();
	
	int finalHeight = expanded ? calculateContentHeight() : 0;
	
	auto* contentAnimation = static_cast<QPropertyAnimation*>(m_toggleAnimation->animationAt(0));
	
	if (expanded)
	{
		contentAnimation->setStartValue(0);
		contentAnimation->setEndValue(finalHeight);
	}
	else
	{
		contentAnimation->setStartValue(m_contentArea->maximumHeight());
		contentAnimation->setEndValue(0);
	}
	
	m_toggleAnimation->setDirection(QAbstractAnimation::Forward);
	m_toggleAnimation->start();
	
	emit toggled(expanded);
}

void CollapsibleSection::updateToggleButton()
{
	QStringList possiblePaths = {
		"plugins/Vibed/downArrow.svg",
		"../plugins/Vibed/downArrow.svg",
		"../../plugins/Vibed/downArrow.svg"
	};
	
	QPixmap arrowPixmap;
	QString svgContent;
	
	for (const QString& svgPath : possiblePaths)
	{
		QFile svgFile(svgPath);
		if (svgFile.exists() && svgFile.open(QIODevice::ReadOnly))
		{
			svgContent = svgFile.readAll();
			svgContent.replace("stroke=\"#ffffff\"", "stroke=\"#707072\"");
			break;
		}
	}
	
	if (svgContent.isEmpty())
	{
		QDir currentDir = QDir::current();
		QStringList searchPaths = {
			currentDir.absoluteFilePath("plugins/Vibed/downArrow.svg"),
			currentDir.absoluteFilePath("../plugins/Vibed/downArrow.svg"),
			currentDir.absoluteFilePath("../../plugins/Vibed/downArrow.svg")
		};
		
		for (const QString& absPath : searchPaths)
		{
			QFile svgFile(absPath);
			if (svgFile.exists() && svgFile.open(QIODevice::ReadOnly))
			{
				svgContent = svgFile.readAll();
				svgContent.replace("stroke=\"#ffffff\"", "stroke=\"#707072\"");
				break;
			}
		}
	}
	
	if (!svgContent.isEmpty())
	{
		QSvgRenderer renderer(svgContent.toUtf8());
		if (renderer.isValid())
		{
			QImage image(12, 12, QImage::Format_ARGB32);
			image.fill(Qt::transparent);
			QPainter painter(&image);
			painter.setRenderHint(QPainter::Antialiasing);
			
			if (!m_expanded)
			{
				painter.translate(6, 6);
				painter.rotate(-90); 
				painter.translate(-6, -6);
			}
			
			renderer.render(&painter);
			painter.end();
			
			arrowPixmap = QPixmap::fromImage(image);
		}
	}
	
	if (arrowPixmap.isNull())
	{
		m_arrowLabel->setText(m_expanded ? "▼" : "▶");
		m_arrowLabel->setStyleSheet(
			"QPushButton {"
			"  color: #707072;"
			"  font-size: 10px;"
			"  background-color: transparent;"
			"}"
		);
		m_arrowLabel->setIcon(QIcon());
	}
	else
	{
		m_arrowLabel->setIcon(QIcon(arrowPixmap));
		m_arrowLabel->setIconSize(QSize(12, 12));
		m_arrowLabel->setText("");
	}
}

} // namespace lmms::gui
