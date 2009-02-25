#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

/***************************************************************************
 *   Copyright (C) 2007-2009 Sergio Pistone (sergio_pistone@yahoo.com.ar)  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
 *   Boston, MA 02110-1301, USA.                                           *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

#include <KDialog>

class QLabel;
class QProgressBar;
class QCloseEvent;

namespace SubtitleComposer
{
	class ProgressDialog : public KDialog
	{
		Q_OBJECT

		public:

			ProgressDialog( const QString& caption, const QString& description, QWidget* parent=0 );

			int value() const;
			int minimum() const;
			int maximum() const;
			QString description() const;

		protected:

			virtual void closeEvent( QCloseEvent* event );

		public slots:

			void setMinimum( int minimum );
			void incrementMinimum( int delta );
			void setMaximum( int maximum );
			void incrementMaximum( int delta );
			void setValue( int value );
			void setDescription( const QString& description );

		private:

			QLabel* m_label;
			QProgressBar* m_progressBar;
	};
}

#endif
