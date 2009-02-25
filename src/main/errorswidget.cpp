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

#include "errorswidget.h"
#include "application.h"
#include "configs/errorsconfig.h"
#include "../core/subtitle.h"
#include "../core/subtitleline.h"

#include <climits>

#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QHeaderView>

#include <KMenu>
#include <KConfig>
#include <KConfigGroup>
#include <KDebug>
#include <KLocale>

using namespace SubtitleComposer;

/// ERRORS MODEL NODE
/// =================

ErrorsModelNode::ErrorsModelNode( ErrorsModel* model, const SubtitleLine* line, int index ):
	m_model( model ),
	m_line( line ),
	m_itemCount( 0 ),
	m_errorsCount( 0 ),
	m_marked( false )
{
	update( index );
}

ErrorsModelNode::~ErrorsModelNode()
{
	m_model->m_errorsCount -= m_errorsCount;
	if ( m_marked )
		m_model->m_marksCount--;
	if ( m_itemCount )
		m_model->m_linesWithIssuesCount--;
}

/// code taken from http://tekpool.wordpress.com/category/bit-count/
int ErrorsModelNode::bitsCount( unsigned int u )
{
	unsigned int uCount;
	uCount = u - ((u >> 1) & 033333333333) - ((u >> 2) & 011111111111);
	return ((uCount + (uCount >> 3)) & 030707070707) % 63;
}

void ErrorsModelNode::update( int lineIndex )
{
	bool newMarked = m_line->errorFlags() & SubtitleLine::UserMark;
	int newErrorsCount = bitsCount( m_line->errorFlags() );
	if ( newMarked )
		newErrorsCount--;

	if ( m_marked != newMarked )
	{
		m_model->incrementMarksCount( newMarked ? 1 : -1 );
		m_marked = newMarked;
	}

	if ( m_errorsCount != newErrorsCount )
	{
		m_model->incrementErrorsCount( newErrorsCount - m_errorsCount );
		m_errorsCount = newErrorsCount;
	}

	int newItemsCount = m_errorsCount + (m_marked ? 1 : 0);

	if ( newItemsCount > m_itemCount )
	{
		int modelL1Row = m_model->mapLineIndexToModelL1Row( lineIndex ) + 1;

		if ( ! m_itemCount ) // line was not visible before
		{
			m_model->beginInsertRows( QModelIndex(), modelL1Row, modelL1Row );

			m_model->incrementVisibleLinesCount( 1 );

			m_model->endInsertRows();
		}

		m_model->beginInsertRows( m_model->index( modelL1Row, 0 ), m_itemCount, newItemsCount - 1 );

		m_itemCount = newItemsCount;

		m_model->endInsertRows();
	}
	else if ( newItemsCount < m_itemCount )
	{
		int modelL1Row = m_model->mapLineIndexToModelL1Row( lineIndex );

		if ( ! newItemsCount ) // line is no longer visible
		{
			m_model->beginRemoveRows( QModelIndex(), modelL1Row, modelL1Row );

			m_model->incrementVisibleLinesCount( -1 );
			m_itemCount = 0;

			m_model->endRemoveRows();
		}
		else
		{
			m_model->beginRemoveRows( m_model->index( modelL1Row, 0 ), newItemsCount, m_itemCount - 1 );

			m_itemCount = newItemsCount;

			m_model->endRemoveRows();
		}
	}
}




/// ERRORS MODEL
/// ============

ErrorsModel::ErrorsModel( QObject* parent ):
	QAbstractItemModel( parent ),
	m_subtitle( 0 ),
	m_nodes(),
	m_statsChangedTimer( new QTimer( this ) ),
	m_linesWithIssuesCount( 0 ),
	m_errorsCount( 0 ),
	m_marksCount( 0 ),
	m_dataChangedTimer( new QTimer( this ) ),
	m_minChangedLineIndex( -1 ),
	m_maxChangedLineIndex( -1 )
{
	onErrorsOptionChanged();

	connect( app()->errorsConfig(), SIGNAL(optionChanged(const QString&,const QString&)),
			 this, SLOT(onErrorsOptionChanged()) );

	m_statsChangedTimer->setInterval( 0 );
	m_statsChangedTimer->setSingleShot( true );

	connect( m_statsChangedTimer, SIGNAL( timeout() ), this, SIGNAL( statsChanged() ) );

	m_dataChangedTimer->setInterval( 0 );
	m_dataChangedTimer->setSingleShot( true );

	connect( m_dataChangedTimer, SIGNAL( timeout() ), this, SLOT( emitDataChanged() ) );
}

ErrorsModel::~ErrorsModel()
{
	qDeleteAll( m_nodes );
}

int ErrorsModel::mapModelIndexToLineIndex( const QModelIndex& modelIndex ) const
{
	if ( ! modelIndex.isValid() ) // modelIndex is the root item
		return -1;
	else if ( modelIndex.parent().isValid() ) // modelIndex is an error item
		return mapModelL1RowToLineIndex( modelIndex.parent().row() );
	else // modelIndex is a line item
		return mapModelL1RowToLineIndex( modelIndex.row() );
}

int ErrorsModel::mapLineIndexToModelL1Row( int targetLineIndex ) const
{
	int modelL1Row = -1;
	for ( int lineIndex = 0; lineIndex <= targetLineIndex; ++lineIndex )
		if ( m_nodes.at( lineIndex )->isVisible() )
			modelL1Row++;

	return modelL1Row;
}

int ErrorsModel::mapModelL1RowToLineIndex( int targetModelL1Row ) const
{
	int modelL1Row = -1;
	for ( int lineIndex = 0, linesCount = m_nodes.count(); lineIndex < linesCount; ++lineIndex )
	{
		if ( m_nodes.at( lineIndex )->isVisible() )
		{
			modelL1Row++;
			if ( modelL1Row == targetModelL1Row )
				return lineIndex;
		}
	}

	return -1;
}

int ErrorsModel::mapModelL2RowToLineErrorID( int targetModelL2Row, int lineErrorFlags ) const
{
	for ( int errorID = 0, modelL2Row = -1; errorID < SubtitleLine::ErrorSIZE; ++errorID )
	{
		if ( ((0x1 << errorID) & lineErrorFlags) )
		{
			modelL2Row++;
			if ( modelL2Row == targetModelL2Row )
				return errorID;
		}
	}

	return SubtitleLine::ErrorUNKNOWN;
}

int ErrorsModel::mapModelL2RowToLineErrorFlag( int targetModelL2Row, int lineErrorFlags ) const
{
	for ( int errorID = 0, modelL2Row = -1; errorID < SubtitleLine::ErrorSIZE; ++errorID )
	{
		if ( ((0x1 << errorID) & lineErrorFlags) )
		{
			modelL2Row++;
			if ( modelL2Row == targetModelL2Row )
				return 0x1 << errorID;
		}
	}

	return 0;
}


void ErrorsModel::onLinesInserted( int firstLineIndex, int lastLineIndex )
{
// 	PROFILE();

	int firstVisibleLineIndex = INT_MAX, lastVisibleLineIndex = INT_MIN;

	ErrorsModelNode* lineErrorsData;
	for ( int lineIndex = firstLineIndex; lineIndex <= lastLineIndex; ++lineIndex )
	{
		lineErrorsData = new ErrorsModelNode( this, m_subtitle->line( lineIndex ), lineIndex );
		m_nodes.insert( lineIndex, lineErrorsData );
		if ( lineErrorsData->isVisible() )
		{
			if ( lineIndex < firstVisibleLineIndex )
				firstVisibleLineIndex = lineIndex;
			if ( lineIndex > lastVisibleLineIndex )
				lastVisibleLineIndex = lineIndex;
		}
	}

	if ( firstVisibleLineIndex != INT_MAX && lastVisibleLineIndex != INT_MIN ) // at least one visible line was inserted
	{
		beginInsertRows(
			QModelIndex(),
			mapLineIndexToModelL1Row( firstVisibleLineIndex ),
			mapLineIndexToModelL1Row( lastVisibleLineIndex )
		);
		endInsertRows();
	}

	emit dataChanged();
}

void ErrorsModel::onLinesRemoved( int firstLineIndex, int lastLineIndex )
{
// 	PROFILE();

	int firstVisibleLineIndex = INT_MAX, lastVisibleLineIndex = INT_MIN;

	for ( int lineIndex = firstLineIndex; lineIndex <= lastLineIndex; ++lineIndex )
	{
		if ( m_nodes.at( lineIndex )->isVisible() )
		{
			if ( lineIndex < firstVisibleLineIndex )
				firstVisibleLineIndex = lineIndex;
			if ( lineIndex > lastVisibleLineIndex )
				lastVisibleLineIndex = lineIndex;
		}
	}

	int lastErrorRow = mapLineIndexToModelL1Row( lastVisibleLineIndex );

	if ( firstVisibleLineIndex != INT_MAX && lastVisibleLineIndex != INT_MIN ) // at least one visible line was removed
	{
		int firstErrorRow = mapLineIndexToModelL1Row( firstVisibleLineIndex );

		beginRemoveRows( QModelIndex(), firstErrorRow, lastErrorRow );
		endRemoveRows();
	}

	for ( int lineIndex = firstLineIndex; lineIndex <= lastLineIndex; ++lineIndex )
		delete m_nodes.takeAt( firstLineIndex );

	emit dataChanged();
}

void ErrorsModel::onLineErrorsChanged( SubtitleLine* line )
{
	int lineIndex = line->index();

	m_nodes.at( lineIndex )->update( lineIndex );

	markLineChanged( lineIndex );
}

void ErrorsModel::onLinePrimaryTextChanged( SubtitleLine* line )
{
	if ( m_autoClearFixed )
		updateLineErrors( line, line->errorFlags() & SubtitleLine::PrimaryOnlyErrors );

	markLineChanged( line->index() );
}

void ErrorsModel::onLineSecondaryTextChanged( SubtitleLine* line )
{
	if ( m_autoClearFixed )
		updateLineErrors( line, line->errorFlags() & SubtitleLine::SecondaryOnlyErrors );

	markLineChanged( line->index() );
}

void ErrorsModel::onLineTimesChanged( SubtitleLine* line )
{
	if ( m_autoClearFixed )
	{
		updateLineErrors( line, line->errorFlags() & SubtitleLine::TimesErrors );
		SubtitleLine* prevLine = line->prevLine();
		if ( prevLine )
			updateLineErrors( prevLine, prevLine->errorFlags() & SubtitleLine::OverlapsWithNext );
	}

	markLineChanged( line->index() );
}

void ErrorsModel::updateLineErrors( SubtitleLine* line, int errorFlags )
{
	line->check(
		errorFlags,
		m_minDuration,
		m_maxDuration,
		m_minDurationPerChar,
		m_maxDurationPerChar,
		m_maxCharacters,
		m_maxLines
	);
}

void ErrorsModel::markLineChanged( int lineIndex )
{
	if ( m_minChangedLineIndex < 0 )
	{
		m_minChangedLineIndex = lineIndex;
		m_maxChangedLineIndex = lineIndex;
		m_dataChangedTimer->start();
	}
	else if ( lineIndex < m_minChangedLineIndex )
		m_minChangedLineIndex = lineIndex;
	else if ( lineIndex > m_maxChangedLineIndex )
		m_maxChangedLineIndex = lineIndex;
}

void ErrorsModel::emitDataChanged()
{
	if ( m_minChangedLineIndex < 0 )
		m_minChangedLineIndex = m_maxChangedLineIndex;
	else if ( m_maxChangedLineIndex < 0 )
		m_maxChangedLineIndex = m_minChangedLineIndex;

	// at this point m_minChangedLineIndex and m_maxChangedLineIndex contain
	// indexes for lines that may not be visible
	int firstVisibleLineIndex = INT_MAX, lastVisibleLineIndex = INT_MIN;
	for ( int lineIndex = m_minChangedLineIndex; lineIndex <= m_maxChangedLineIndex; ++lineIndex )
	{
		if ( m_nodes.at( lineIndex )->isVisible() )
		{
			if ( lineIndex < firstVisibleLineIndex )
				firstVisibleLineIndex = lineIndex;
			if ( lineIndex > lastVisibleLineIndex )
				lastVisibleLineIndex = lineIndex;
		}
	}

	if ( firstVisibleLineIndex != INT_MAX && lastVisibleLineIndex != INT_MIN ) // at least one visible line was removed
	{
		// now we can map the lines indexes to error indexes
		emit dataChanged(
			index( mapLineIndexToModelL1Row( firstVisibleLineIndex ), 0 ),
			index( mapLineIndexToModelL1Row( lastVisibleLineIndex ), 0 )
		);
	}

	m_minChangedLineIndex = -1;
	m_maxChangedLineIndex = -1;
}

void ErrorsModel::incrementVisibleLinesCount( int delta )
{
	if ( ! m_statsChangedTimer->isActive() )
		m_statsChangedTimer->start();
	m_linesWithIssuesCount += delta;
}

void ErrorsModel::incrementErrorsCount( int delta )
{
	if ( ! m_statsChangedTimer->isActive() )
		m_statsChangedTimer->start();
	m_errorsCount += delta;
}

void ErrorsModel::incrementMarksCount( int delta )
{
	if ( ! m_statsChangedTimer->isActive() )
		m_statsChangedTimer->start();
	m_marksCount += delta;
}

void ErrorsModel::onErrorsOptionChanged()
{
	ErrorsConfig* errorsConfig = app()->errorsConfig();

	m_autoClearFixed = errorsConfig->autoClearFixed();
	m_minDuration = errorsConfig->minDuration();
	m_maxDuration = errorsConfig->maxDuration();
	m_minDurationPerChar = errorsConfig->minDurationPerChar();
	m_maxDurationPerChar = errorsConfig->maxDurationPerChar();
	m_maxCharacters = errorsConfig->maxCharacters();
	m_maxLines = errorsConfig->maxLines();
}

void ErrorsModel::setSubtitle( Subtitle* subtitle )
{
	if ( m_subtitle != subtitle )
	{
		if ( m_subtitle )
		{
			disconnect( m_subtitle, SIGNAL( linesInserted( int, int ) ),
						this, SLOT( onLinesInserted( int, int ) ) );
			disconnect( m_subtitle, SIGNAL( linesRemoved( int, int ) ),
						this, SLOT( onLinesRemoved( int, int ) ) );

			disconnect( m_subtitle, SIGNAL( lineErrorFlagsChanged( SubtitleLine*, int ) ),
						this, SLOT( onLineErrorsChanged( SubtitleLine* ) ) );

			disconnect( m_subtitle, SIGNAL( linePrimaryTextChanged( SubtitleLine*, const SString& ) ),
						this, SLOT( onLinePrimaryTextChanged( SubtitleLine* ) ) );
			disconnect( m_subtitle, SIGNAL( lineSecondaryTextChanged( SubtitleLine*, const SString& ) ),
						this, SLOT( onLineSecondaryTextChanged( SubtitleLine* ) ) );
			disconnect( m_subtitle, SIGNAL( lineShowTimeChanged( SubtitleLine*, const Time& ) ),
						this, SLOT( onLineTimesChanged( SubtitleLine* ) ) );
			disconnect( m_subtitle, SIGNAL( lineHideTimeChanged( SubtitleLine*, const Time& ) ),
						this, SLOT( onLineTimesChanged( SubtitleLine* ) ) );

			if ( m_subtitle->linesCount() )
				onLinesRemoved( 0, m_subtitle->linesCount() - 1 );
		}

		m_subtitle = subtitle;

		if ( m_subtitle )
		{
			if ( m_subtitle->linesCount() )
				onLinesInserted( 0, m_subtitle->linesCount() - 1 );

			connect( m_subtitle, SIGNAL( linesInserted( int, int ) ),
					 this, SLOT( onLinesInserted( int, int ) ) );
			connect( m_subtitle, SIGNAL( linesRemoved( int, int ) ),
					 this, SLOT( onLinesRemoved( int, int ) ) );

			connect( m_subtitle, SIGNAL( lineErrorFlagsChanged( SubtitleLine*, int ) ),
					 this, SLOT( onLineErrorsChanged( SubtitleLine* ) ) );

			connect( m_subtitle, SIGNAL( linePrimaryTextChanged( SubtitleLine*, const SString& ) ),
					 this, SLOT( onLinePrimaryTextChanged( SubtitleLine* ) ) );
			connect( m_subtitle, SIGNAL( lineSecondaryTextChanged( SubtitleLine*, const SString& ) ),
					 this, SLOT( onLineSecondaryTextChanged( SubtitleLine* ) ) );
			connect( m_subtitle, SIGNAL( lineShowTimeChanged( SubtitleLine*, const Time& ) ),
					 this, SLOT( onLineTimesChanged( SubtitleLine* ) ) );
			connect( m_subtitle, SIGNAL( lineHideTimeChanged( SubtitleLine*, const Time& ) ),
					 this, SLOT( onLineTimesChanged( SubtitleLine* ) ) );
		}
	}
}

int ErrorsModel::rowCount( const QModelIndex& parent ) const
{
	if ( ! m_subtitle )
		return 0;

	if ( ! parent.isValid() ) // parent is the root item
		return m_linesWithIssuesCount;

	if ( parent.parent().isValid() ) // parent is an error item
		return 0;

	// parent is a line item
	int lineIndex = mapModelL1RowToLineIndex( parent.row() );
	return lineIndex < 0 ? 0 : m_nodes.at( lineIndex )->itemCount();
}

int ErrorsModel::columnCount( const QModelIndex& /*parent*/ ) const
{
	return ColumnCount;
}

QModelIndex ErrorsModel::index( int row, int column, const QModelIndex& parent ) const
{
	SubtitleLine* line = static_cast<SubtitleLine*>( parent.internalPointer() );

	if ( ! line ) // index is a root level item
	{
		if ( row < 0 || row >= m_linesWithIssuesCount )
			return QModelIndex();

// 		kDebug() << "retrieved level 1 CHILD index " << row;

		return createIndex( row, column, (void*)&LEVEL1_LINE );
	}
	if ( line == &LEVEL1_LINE ) // index is a first level item (line item)
	{
		if ( row >= rowCount( parent ) || row < 0 /*|| column != Number*/ )
			return QModelIndex();

// 		kDebug() << "retrieved level 2 CHILD index " << row;

		return createIndex( row, column, m_subtitle->line( mapModelL1RowToLineIndex( parent.row() ) ) );
	}
	else // if ( line ) // index is a second level item (error item)
	{
// 		kDebug() << "attempted to retrieve level 3 CHILD index " << row;

		return QModelIndex();
	}
}

QModelIndex ErrorsModel::parent( const QModelIndex& index ) const
{
	SubtitleLine* line = static_cast<SubtitleLine*>( index.internalPointer() );

	if ( ! line ) // index is a root level item
	{
// 		kDebug() << "attempted to retrieve level -1 PARENT index" << -2;

		return QModelIndex();
	}
	if ( line == &LEVEL1_LINE ) // index is a first level item (line item)
	{
// 		kDebug() << "retrieved level 0 PARENT index" << -1;

		return QModelIndex();
	}
	else // if ( line ) // index is a second level item (error item)
	{
// 		kDebug() << "retrieved level 1 PARENT index" << line->index();

		return createIndex( mapLineIndexToModelL1Row( line->index() ), 0, (void*)&LEVEL1_LINE );
	}
}

QVariant ErrorsModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
	if ( orientation == Qt::Vertical || role != Qt::DisplayRole )
		return QVariant();

	switch ( section )
	{
		case Number:	return i18n( "Line Issues" );
		case Errors:	return i18n( "Errors" );
		case Marks:		return i18n( "Marks" );
		default:		return QVariant();
	}
}

QVariant ErrorsModel::data( const QModelIndex& index, int role ) const
{
	if ( ! m_subtitle || ! index.isValid() )
		return QVariant();

	SubtitleLine* line = static_cast<SubtitleLine*>( index.internalPointer() );

	if ( line == &LEVEL1_LINE )
	{
		if ( role == Qt::DisplayRole )
		{
			static const QString space( ' ' );

			int lineIndex = mapModelL1RowToLineIndex( index.row() );
			if ( lineIndex < 0 )
				return QVariant();

			if ( index.column() == Number )
				return i18n( "Line <numid>%1</numid>", lineIndex + 1 );
			else if ( index.column() == Errors )
				return QString::number( m_nodes.at( lineIndex )->errorsCount() );
			else if ( index.column() == Marks )
				return QString( m_nodes.at( lineIndex )->isMarked() ? "1" : "0" );
			else
				return QVariant();
		}
		else
			return QVariant();
	}
	else
	{
		if ( index.column() != Number )
			return QVariant();

		if ( role == Qt::DisplayRole )
		{
			int errorID = mapModelL2RowToLineErrorID( index.row(), line->errorFlags() );
			return line->fullErrorText( (SubtitleLine::ErrorID)errorID );
		}
		else if ( role == Qt::DecorationRole )
		{
			int errorID = mapModelL2RowToLineErrorID( index.row(), line->errorFlags() );
			if ( ((0x1 << errorID) & line->errorFlags()) == 0 )
				return QVariant();
			return errorID == SubtitleLine::UserMarkID ? markIcon() : errorIcon();
		}
		else
			return QVariant();
	}
}

const QIcon& ErrorsModel::markIcon()
{
	static QIcon markIcon;

	if ( markIcon.isNull() )
		markIcon = KIcon( "dialog-warning" );

	return markIcon;
}

const QIcon& ErrorsModel::errorIcon()
{
	static QIcon errorIcon;

	if ( errorIcon.isNull() )
		errorIcon = KIcon( "dialog-error" );

	return errorIcon;
}




/// ERRORS WIDGET
/// =============

ErrorsWidget::ErrorsWidget( QWidget* parent ):
	TreeView( parent )
{
	setModel( new ErrorsModel( this ) );

	QHeaderView* header = this->header();
	header->setResizeMode( ErrorsModel::Number, QHeaderView::Interactive );
	header->setResizeMode( ErrorsModel::Errors, QHeaderView::Interactive );
	header->setResizeMode( ErrorsModel::Marks, QHeaderView::Interactive );
// 	header->setResizeMode( ErrorsModel::Errors, QHeaderView::ResizeToContents );
// 	header->setResizeMode( ErrorsModel::Marks, QHeaderView::ResizeToContents );

	setSortingEnabled( false );
	setRootIsDecorated( true );
	setAllColumnsShowFocus( true );
	setIconSize( QSize( 16, 16 ) );
	setIndentation( 15 );

	setSelectionMode( QAbstractItemView::ExtendedSelection );
	setSelectionBehavior( QAbstractItemView::SelectRows );

	connect( selectionModel(), SIGNAL( currentRowChanged( const QModelIndex&, const QModelIndex& ) ),
			 this, SLOT( onCurrentRowChanged( const QModelIndex& ) ) );
}

ErrorsWidget::~ErrorsWidget()
{
}

void ErrorsWidget::loadConfig()
{
	KConfigGroup group( KGlobal::config()->group( "ErrorsWidget Settings" ) );

	QByteArray state;
	QStringList strState = group.readXdgListEntry( "Columns State", QString( "" ).split( ' ' ) );
	for ( QStringList::ConstIterator it = strState.begin(), end = strState.end(); it != end; ++it )
		state.append( (char)(*it).toInt() );
	header()->restoreState( state );
}

void ErrorsWidget::saveConfig()
{
	KConfigGroup group( KGlobal::config()->group( "ErrorsWidget Settings" ) );

	QStringList strState;
	QByteArray state = header()->saveState();
	for ( int index = 0, size = state.size(); index < size; ++index )
		strState.append( QString::number( state[index] ) );
	group.writeXdgListEntry( "Columns State", strState );
}

void ErrorsWidget::setSubtitle( Subtitle* subtitle )
{
	model()->setSubtitle( subtitle );
}

void ErrorsWidget::mouseDoubleClickEvent( QMouseEvent* e )
{
	QModelIndex index = indexAt( viewport()->mapFromGlobal( e->globalPos() ) );
	if ( index.isValid() )
		emit lineDoubleClicked( model()->subtitle()->line( model()->mapModelIndexToLineIndex( index ) ) );
}

void ErrorsWidget::contextMenuEvent( QContextMenuEvent* e )
{
	int linesWithIssuesCount = 0, errorsCount = 0, marksCount = 0;

	QItemSelectionModel* selection = selectionModel();
	for ( int lineIndex=0, modelL1Row=0, linesCount=model()->subtitle()->linesCount(); lineIndex < linesCount; ++lineIndex )
	{
		const ErrorsModelNode* node = model()->node( lineIndex );

		if ( node->isVisible() )
		{
			QModelIndex modelL1Index = model()->index( modelL1Row, 0 );
			if ( selection->isSelected( modelL1Index ) )
			{
				linesWithIssuesCount++;
				errorsCount += node->errorsCount();
				if ( node->isMarked() )
					marksCount++;
			}
			else
			{
				bool selected = false;
				int lineErrorFlags = node->line()->errorFlags();
				for ( int modelL2Row = 0, modelL2RowCount = node->itemCount(); modelL2Row < modelL2RowCount; ++modelL2Row )
				{
					if ( selection->isSelected( model()->index( modelL2Row, 0, modelL1Index ) ) )
					{
						selected = true;
						if ( model()->mapModelL2RowToLineErrorID( modelL2Row, lineErrorFlags ) == SubtitleLine::UserMarkID )
							marksCount++;
						else
							errorsCount++;
					}
				}
				if ( selected )
					linesWithIssuesCount++;
			}

			modelL1Row++;

			if ( linesWithIssuesCount > 1 && errorsCount > 1 && linesCount > 1 )
				break;
		}
	}

	if ( linesWithIssuesCount || errorsCount || marksCount )
	{
		KMenu menu;
		Application* app = Application::instance();

		menu.addAction( i18n( "Expand All" ), this, SLOT( expandAll() ) );
		menu.addAction( i18n( "Collapse All" ), this, SLOT( collapseAll() ) );

		menu.addSeparator();

		menu.addAction( i18n( "Clear Selected Marks" ), app, SLOT( clearSelectedMarks() ) )->setEnabled( marksCount );
		menu.addAction( i18n( "Clear Selected Errors" ), app, SLOT( clearSelectedErrors() ) )->setEnabled( errorsCount );

		if ( ! app->errorsConfig()->autoClearFixed() )
		{
			menu.addSeparator();

			menu.addAction( i18n( "Clear Fixed Selection" ), app, SLOT( recheckSelectedErrors() ) )->setEnabled( errorsCount );
		}

		menu.exec( e->globalPos() );
		e->ignore();
	}

	TreeView::contextMenuEvent( e );
}

SubtitleLine* ErrorsWidget::currentLine()
{
	QModelIndex currentIndex = this->currentIndex();

	if ( ! currentIndex.isValid() )
		return 0;

	if ( currentIndex.parent().isValid() )
		currentIndex = currentIndex.parent();

	return model()->subtitle()->line( currentIndex.row() );
}

int ErrorsWidget::lineSelectedErrorFlags( int lineIndex )
{
	QItemSelectionModel* selection = selectionModel();
	int modelL1Row = model()->mapLineIndexToModelL1Row( lineIndex );

	if ( modelL1Row < 0 )
		return 0;

	int lineErrorFlags = model()->subtitle()->line( lineIndex )->errorFlags();

	QModelIndex modelL1Index = model()->index( modelL1Row, 0 );
	if ( selection->isSelected( modelL1Index ) )
		return lineErrorFlags;

	int selectedErrorFlags = 0;

	for ( int modelL2row = 0, modelL2RowCount = model()->rowCount( modelL1Index ); modelL2row < modelL2RowCount; ++modelL2row )
	{
		if ( selection->isSelected( model()->index( modelL2row, 0, modelL1Index ) ) )
			selectedErrorFlags |= model()->mapModelL2RowToLineErrorFlag( modelL2row, lineErrorFlags );
	}

	return selectedErrorFlags;
}

RangeList ErrorsWidget::selectionRanges() const
{
	RangeList ranges;

	QItemSelectionModel* selection = selectionModel();

	int rangeFirstIndex = -1, modelL1Row = 0;
	int lineIndex = 0;
	for ( int linesCount = model()->subtitle()->linesCount(); lineIndex < linesCount; ++lineIndex )
	{
		const ErrorsModelNode* node = model()->node( lineIndex );
		if ( node->isVisible() )
		{
			QModelIndex modelL1Index = model()->index( modelL1Row, 0 );
			bool selected = selection->isSelected( modelL1Index );

			if ( ! selected )
			{
 				// is one of modelL1Index children selected?
				for ( int modelL2Row = 0, modelL2RowCount = node->itemCount(); modelL2Row < modelL2RowCount; ++modelL2Row )
				{
					if ( selection->isSelected( model()->index( modelL2Row, 0, modelL1Index ) ) )
					{
						selected = true;
						break;
					}
				}
			}

			if ( selected )
			{
				if ( rangeFirstIndex == -1 ) // mark start of selected range
					rangeFirstIndex = lineIndex;
			}
			else
			{
				if ( rangeFirstIndex != -1 )
				{
					ranges << Range( rangeFirstIndex, lineIndex - 1 );
					rangeFirstIndex = -1;
				}
			}
			modelL1Row++;
		}
	}

	if ( rangeFirstIndex != -1 )
		ranges << Range( rangeFirstIndex, lineIndex - 1 );

	return ranges;
}

void ErrorsWidget::setCurrentLine( SubtitleLine* line, bool clearSelection )
{
	if ( line )
	{
		int modelL1Row = model()->mapLineIndexToModelL1Row( line->index() );
		if ( modelL1Row >= 0 )
			selectionModel()->setCurrentIndex(
				model()->index( modelL1Row, 0 ),
				clearSelection ?
					QItemSelectionModel::Select | QItemSelectionModel::Rows | QItemSelectionModel::Clear :
					QItemSelectionModel::Select | QItemSelectionModel::Rows
			);
	}
}

void ErrorsWidget::onCurrentRowChanged( const QModelIndex& currentIndex )
{
	int currentLineIndex = model()->mapModelIndexToLineIndex( currentIndex );
	emit currentLineChanged( currentLineIndex < 0 ? 0 : model()->subtitle()->line( currentLineIndex ) );
}

void ErrorsWidget::keyPressEvent( QKeyEvent* event )
{
	if ( event->matches( QKeySequence::Delete ))
	{
		SubtitleCompositeActionExecutor executor( *model()->subtitle(), i18n( "Clear Lines Errors" ) );

		app()->clearSelectedErrors();
		app()->clearSelectedMarks();

		event->accept();
	}
	else
		TreeView::keyPressEvent( event );
}

#include "errorswidget.moc"
