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

#include "savesubtitledialog.h"
#include "../application.h"
#include "../../common/filesavehelper.h"
#include "../../formats/formatmanager.h"

#include <QtGui/QLabel>
#include <QtGui/QGridLayout>

#include <KLocale>
#include <KMessageBox>
#include <kabstractfilewidget.h>
#include <KFileFilterCombo>
#include <KComboBox>

using namespace SubtitleComposer;

SaveSubtitleDialog::SaveSubtitleDialog( bool primary, const QString& startDir, const QString& encoding, int newLine, const QString& format, QWidget* parent ):
	KFileDialog( startDir, outputFormatsFilter(), parent )
{
	setCaption( primary ? i18n( "Save Subtitle" ) : i18n( "Save Translation Subtitle" ) );
	setOperationMode( KFileDialog::Saving );

	setModal( true );
	setMode( KFile::File );

	filterWidget()->setEditable( false );

	if ( FormatManager::instance().output( format ) )
		setCurrentFilter( format );
	else
		setCurrentFilter( FormatManager::instance().outputNames().first() );

	// setting the current filter will force the first valid extension for the format which
	// may not be the one of the file (even when the file's extension is perfectly valid)
	setSelection( startDir );

	QStringList newLineModes;
	newLineModes << "UNIX";
	newLineModes << "Windows";
	newLineModes << "Macintosh";

	QWidget* customWidget = new QWidget( this );

	m_encodingComboBox = new KComboBox( customWidget );
	m_encodingComboBox->addItems( app()->availableEncodingNames() );
	m_encodingComboBox->setCurrentItem( encoding );
	m_encodingComboBox->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );

	QLabel* newLineLabel = new QLabel( customWidget );
	newLineLabel->setText( i18n( "EOL:" ) );
	newLineLabel->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );

	m_newLineComboBox = new KComboBox( customWidget );
	m_newLineComboBox->addItems( newLineModes );
	m_newLineComboBox->setCurrentIndex( newLine );
	m_newLineComboBox->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Fixed );

	QGridLayout* layout = new QGridLayout( customWidget );
	layout->setContentsMargins( 0, 0, 0, 0 );
	layout->addWidget( m_encodingComboBox, 0, 0 );
	layout->addWidget( newLineLabel, 0, 1 );
	layout->addWidget( m_newLineComboBox, 0, 2 );

	fileWidget()->setCustomWidget( i18n( "Encoding:" ), customWidget );
}

QString SaveSubtitleDialog::selectedEncoding() const
{
	return m_encodingComboBox->currentText();
}

void SaveSubtitleDialog::setCurrentFilter( const QString& formatName )
{
	const OutputFormat* format = FormatManager::instance().output( formatName );
	if ( format )
	{
		QStringList extensions( format->extensions() );
		QString filter;
		for ( QStringList::ConstIterator it = extensions.begin(), end = extensions.end(); it != end; ++it )
			filter += "*." + *it + " *." + (*it).toUpper();
		filter += '|' + formatName;

		filterWidget()->setCurrentFilter( filter );
	}
}

QString SaveSubtitleDialog::selectedFormat() const
{
	return filterWidget()->currentText();
}

Format::NewLine SaveSubtitleDialog::selectedNewLine() const
{
	return m_newLineComboBox ? (Format::NewLine)m_newLineComboBox->currentIndex() : Format::CurrentOS;
}

QString SaveSubtitleDialog::outputFormatsFilter()
{
	static QString filter;

	if ( filter.isEmpty() )
	{
		QStringList formats = FormatManager::instance().outputNames();
		for ( QStringList::ConstIterator it = formats.begin(), end = formats.end(); it != end; ++it )
		{
			const OutputFormat* format = FormatManager::instance().output( *it );
			const QStringList& formatExtensions = format->extensions();
			QString extensions;
			for ( QStringList::ConstIterator extIt = formatExtensions.begin(), extEnd = formatExtensions.end(); extIt != extEnd; ++extIt )
				extensions += "*." + *extIt + " *." + (*extIt).toUpper();
			filter += '\n' + extensions + '|' + format->name();
		}

		filter = filter.trimmed();
	}

	return filter;
}

int SaveSubtitleDialog::exec()
{
	int retCode = KFileDialog::exec();

	if ( retCode == QDialog::Accepted )
	{
		if ( FileSaveHelper::exists( selectedUrl() ) && KMessageBox::warningContinueCancel(
				parentWidget(),
				i18n(
					"A file named \"%1\" already exists. Are you sure you want to overwrite it?",
					QFileInfo( selectedUrl().path() ).fileName()
				),
				i18n( "Overwrite File?" ),
				KGuiItem( i18n( "Overwrite" ) )
			) != KMessageBox::Continue )
			return QDialog::Rejected;
	}

	return retCode;
}

