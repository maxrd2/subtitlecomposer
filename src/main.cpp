/*
 * Copyright (C) 2007-2009 Sergio Pistone <sergio_pistone@yahoo.com.ar>
 * Copyright (C) 2010-2019 Mladen Milinkovic <max@smoothware.net>
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

#include "config.h"
#include "config-subtitlecomposer.h"

#include "application.h"
#include "mainwindow.h"
#include "helpers/commondefs.h"
#include "videoplayer/backend/glrenderer.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QFile>
#include <QDir>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QProcessEnvironment>
#include <QResource>
#include <QMimeDatabase>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

using namespace SubtitleComposer;

static bool
mimeIsMedia(const QMimeType &mime)
{
	const static QStringList mimeList = {
		"video/webm", "video/x-msvideo", "video/mp2t", "application/x-matroska",
		"video/x-matroska", "video/mp4", "video/mpeg", "video/ogg"
	};
	for(const QString &m: mimeList) {
		if(mime.inherits(m))
			return true;
	}
	return false;
}

static bool
mimeIsSubtitle(const QMimeType &mime)
{
	const static QStringList mimeList = {
		"application/x-subrip", "text/x-ssa", "text/x-ass","text/x-microdvd",
		"text/x-mpsub", "text/x-subviewer", "text/x-mplsub", "text/x-tmplayer",
		"text/x-tmplayer+", "application/x-vobsub"
	};
	for(const QString &m: mimeList) {
		if(mime.inherits(m))
			return true;
	}
	return false;
}

static void
handleCommandLine(SubtitleComposer::Application &app, KAboutData &aboutData)
{
	QCommandLineParser parser;
	aboutData.setupCommandLine(&parser);
	parser.setApplicationDescription(aboutData.shortDescription());

	QCommandLineOption subtitleOption("subtitle",
		i18n("The primary subtitle to be edited."),
		"file name");
	parser.addOption(subtitleOption);

	QCommandLineOption videoOption("video",
		i18n("The video to be subtitled."),
		"file name");
	parser.addOption(videoOption);

	QCommandLineOption translationOption("translation",
		i18n("The translation subtitle to be edited.\n"
			"If included, a second pane gets added to the right of the primary subtitle "
			"for side-by-side translation."),
		"file name");
	parser.addOption(translationOption);

	// parse command line
	parser.process(app);
	aboutData.processCommandLine(&parser);

	app.init();

	app.mainWindow()->show();

	QString fileSub;
	QString fileTrans;
	QString fileVideo;

	// load files by specific options
	if(parser.isSet(subtitleOption))
		fileSub = parser.value(subtitleOption);
	if(parser.isSet(videoOption))
		fileVideo = parser.value(videoOption);
	if(parser.isSet(translationOption))
		fileTrans = parser.value(translationOption);

	// load sub/video if positional param is provided
	const QStringList args = parser.positionalArguments();
	for(const QString &arg: args) {
		const QMimeType mime = QMimeDatabase().mimeTypeForFile(arg);
		if(mimeIsSubtitle(mime)) {
			// try to open primary subtitle if not already opened
			if(fileSub.isEmpty()) {
				fileSub = arg;
				continue;
			}
			// try to open translation if not already opened
			if(fileTrans.isEmpty()) {
				fileTrans = arg;
				continue;
			}
		} else if(mimeIsMedia(mime)) {
			// try to open video if not already opened
			if(fileVideo.isEmpty()) {
				fileVideo = arg;
				continue;
			}
		}
	}

	if(!fileSub.isEmpty())
		app.openSubtitle(System::urlFromPath(fileSub));
	else if(!fileTrans.isEmpty())
		app.newSubtitle();
	if(!fileTrans.isEmpty())
		app.openSubtitleTr(System::urlFromPath(fileTrans));
	if(!fileVideo.isEmpty())
		app.openVideo(System::urlFromPath(fileVideo));
}

static void
setupIconTheme(int argc, char **argv)
{
#ifdef SC_BUNDLE_SYSTEM_THEME
	QIcon::setThemeSearchPaths(QIcon::themeSearchPaths() << QStringLiteral(":/icons"));
#endif
	if(QIcon::themeName().isEmpty())
		QIcon::setThemeName(QStringLiteral("breeze"));

	const QStringList fallbackPaths = {
		QStringLiteral(":/icons-fallback/breeze/actions/22"),
		QStringLiteral(":/icons-fallback/breeze/apps/256"),
		QStringLiteral(":/icons-fallback/breeze/apps/128"),
		QStringLiteral(":/icons-fallback/breeze/apps/48"),
		QStringLiteral(":/icons-fallback/breeze/apps/32"),
		QStringLiteral(":/icons-fallback/breeze/apps/16"),
	};

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	QIcon::setFallbackSearchPaths(QIcon::fallbackSearchPaths()
#else
	QIcon::setThemeSearchPaths(QIcon::themeSearchPaths()
#endif
			// access the icons through breeze theme path
			<< QStringLiteral(":/icons-fallback")
			// or directly as fallback
			<< fallbackPaths);

#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
	if(QIcon::fallbackThemeName().isEmpty())
		QIcon::setFallbackThemeName(QStringLiteral("breeze"));
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	// LXQt just ignores QIcon::fallbackSearchPaths()
	QString platformThemeName = QString::fromLocal8Bit(qgetenv("QT_QPA_PLATFORMTHEME"));
	for(int i = 0; i < argc; i++) {
		if(strcmp(argv[i], "-platformtheme") == 0 && ++i < argc) {
			platformThemeName = QString::fromLocal8Bit(argv[i]);
			break;
		}
	}

	if(platformThemeName == QStringLiteral("lxqt"))
		QIcon::setThemeSearchPaths(QIcon::themeSearchPaths() << fallbackPaths);
#endif
}

int
main(int argc, char **argv)
{
	GLRenderer::setupProfile();

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	av_register_all();
	avcodec_register_all();
#endif

	SubtitleComposer::Application app(argc, argv);

	setupIconTheme(argc, argv);

	KLocalizedString::setApplicationDomain("subtitlecomposer");

	KAboutData aboutData(
		QStringLiteral("subtitlecomposer"),
		i18n("Subtitle Composer"),
		SUBTITLECOMPOSER_VERSION_STRING,
		i18n("A KDE subtitle editor."),
		KAboutLicense::GPL_V2,
		QStringLiteral("&copy; 2007-2021 The Subtitle Composer Authors"),
		QString(), // Additional text
		QStringLiteral("https://subtitlecomposer.kde.org/"),
		QStringLiteral("https://invent.kde.org/kde/subtitlecomposer/issues"));

	aboutData.addAuthor(QStringLiteral("Mladen Milinković"), i18n("Maintainer"), "maxrd2@smoothware.net");
	aboutData.addAuthor(QStringLiteral("Sergio Pistone"), i18n("Former Maintainer"), "sergio_pistone@yahoo.com.ar");
	aboutData.addAuthor(QStringLiteral("Thiago Sueto"), i18n("Patches and Website"), "herzenschein@gmail.com");

	aboutData.setTranslator(i18nc("NAME OF TRANSLATORS", "Your names"), i18nc("EMAIL OF TRANSLATORS", "Your emails"));

	aboutData.addCredit(QStringLiteral("Martin Steghöfer"), i18n("code contributions"));
	aboutData.addCredit(QStringLiteral("Marius Kittler"), i18n("code contributions, Arch Linux packaging"));

	aboutData.addCredit(i18n("All people who have contributed and I have forgotten to mention"));

	// register about data
	KAboutData::setApplicationData(aboutData);

	// set app stuff from about data component name
	app.setApplicationName(aboutData.componentName());
	app.setOrganizationDomain(aboutData.organizationDomain());
	app.setApplicationVersion(aboutData.version());
	app.setApplicationDisplayName(aboutData.displayName());
	app.setWindowIcon(QIcon::fromTheme(aboutData.componentName()));

	handleCommandLine(app, aboutData);

	return app.exec();
}
