#ifndef RICHLINEEDIT_H
#define RICHLINEEDIT_H
/*
 * Copyright (C) 2020 Mladen Milinkovic <max@smoothware.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "gui/treeview/richdocumentptr.h"

#include <QBasicTimer>
#include <QStyleOptionViewItem>
#include <QWidget>
#include <QVector>

QT_FORWARD_DECLARE_CLASS(QAction)
QT_FORWARD_DECLARE_CLASS(QEvent)
QT_FORWARD_DECLARE_CLASS(QKeyEvent)

namespace SubtitleComposer {
class RichDocumentEditor;

class RichLineEdit : public QWidget
{
	Q_OBJECT

public:
	explicit RichLineEdit(QWidget *parent = nullptr);
	virtual ~RichLineEdit();

	void setDocument(RichDocument *document);
	inline RichDocument *document() const { return RichDocumentPtr(m_document); }

	inline void setLineEditStyle(const QStyleOptionViewItem &style) { m_lineStyle = style; }

protected:
	bool event(QEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void inputMethodEvent(QInputMethodEvent *e) override;
	QVariant inputMethodQuery(Qt::InputMethodQuery property) const override;
	void focusInEvent(QFocusEvent *e) override;
	void focusOutEvent(QFocusEvent *e) override;
	void changeEvent(QEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
#if QT_CONFIG(draganddrop)
	void dragEnterEvent(QDragEnterEvent *) override;
	void dragMoveEvent(QDragMoveEvent *e) override;
	void dragLeaveEvent(QDragLeaveEvent *e) override;
	void dropEvent(QDropEvent *) override;
#endif

	bool sendMouseEventToInputContext(QMouseEvent *e);
	void setupActions();
	void changeTextColor();

protected:
	QVector<QAction *> m_actions;
	RichDocument *m_document = nullptr;
	QStyleOptionViewItem m_lineStyle;
	RichDocumentEditor *m_control = nullptr;
	QPoint m_mousePressPos;
	int m_mouseYThreshold;
	QBasicTimer m_dndTimer;
	int m_dndCursor = -1;
	QBasicTimer m_tripleClickTimer;
	QPoint m_tripleClickPos;
};

}

#endif // RICHLINEEDIT_H
