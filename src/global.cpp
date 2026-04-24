///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2024 LoRd_MuldeR <MuldeR2@GMX.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// http://www.gnu.org/licenses/gpl-2.0.txt
///////////////////////////////////////////////////////////////////////////////

//x264 includes
#include "global.h"

//Version
#define ENABLE_X264_VERSION_INCLUDE
#include "version.h"
#undef  ENABLE_X264_VERSION_INCLUDE

//MUtils compat
#include "mutils_compat.h"

//Qt includes
#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QUuid>
#include <QMap>
#include <QDate>
#include <QIcon>
#include <QImageReader>
#include <QSharedMemory>
#include <QSysInfo>
#include <QStringList>
#include <QStandardPaths>
#include <QMutex>
#include <QRegularExpression>
#include <QResource>
#include <QTranslator>
#include <QEventLoop>
#include <QTimer>
#include <QLibraryInfo>
#include <QEvent>
#include <QReadLocker>
#include <QWriteLocker>
#include <QProcess>

//C++ includes
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <time.h>
#include <fstream>

//Const
static const char *g_x264_imageformats[] = {"png", "jpg", "gif", "ico", "svg", nullptr};

//Build version
static const struct
{
	unsigned int ver_major;
	unsigned int ver_minor;
	unsigned int ver_patch;
	unsigned int ver_build;
}
g_x264_version =
{
	(VER_X264_MAJOR),
	(VER_X264_MINOR),
	(VER_X264_PATCH),
	(VER_X264_BUILD),
};

//Portable mode
static QReadWriteLock g_portableModeLock;
static bool           g_portableModeData = false;
static bool           g_portableModeInit = false;

//Data path
static QString        g_dataPathData;
static QReadWriteLock g_dataPathLock;

///////////////////////////////////////////////////////////////////////////////
// MACROS
///////////////////////////////////////////////////////////////////////////////

//String helper
#define CLEAN_OUTPUT_STRING(STR) do \
{ \
	const char CTRL_CHARS[3] = { '\r', '\n', '\t' }; \
	for(size_t i = 0; i < 3; i++) \
	{ \
		while(char *pos = strchr((STR), CTRL_CHARS[i])) *pos = char(0x20); \
	} \
} \
while(0)

//String helper
#define TRIM_LEFT(STR) do \
{ \
	const char WHITE_SPACE[4] = { char(0x20), '\r', '\n', '\t' }; \
	for(size_t i = 0; i < 4; i++) \
	{ \
		while(*(STR) == WHITE_SPACE[i]) (STR)++; \
	} \
} \
while(0)

//Check for CLI flag
static inline bool _CHECK_FLAG(const int argc, char **argv, const char *flag)
{
	for(int i = 1; i < argc; i++)
	{
		if(strcasecmp(argv[i], flag) == 0) return true;
	}
	return false;
}

#define CHECK_FLAG(FLAG) _CHECK_FLAG(argc, argv, "--" FLAG)
#define X264_ZERO_MEMORY(X) memset(&(X), 0, sizeof(X))

///////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

/*
 * Version info
 */
unsigned int x264_version_major(void)
{
	return g_x264_version.ver_major;
}

unsigned int x264_version_minor(void)
{
	return (g_x264_version.ver_minor * 10) + (g_x264_version.ver_patch % 10);
}

unsigned int x264_version_build(void)
{
	return g_x264_version.ver_build;
}

/*
 * Check for portable mode
 */
bool x264_is_portable(void)
{
	QReadLocker readLock(&g_portableModeLock);

	if(g_portableModeInit)
	{
		return g_portableModeData;
	}
	
	readLock.unlock();
	QWriteLocker writeLock(&g_portableModeLock);

	if(!g_portableModeInit)
	{
		if(VER_X264_PORTABLE_EDITION)
		{
			qWarning("Simple x264 Launcher portable edition!\n");
			g_portableModeData = true;
		}
		else
		{
			QString baseName = QFileInfo(QApplication::applicationFilePath()).completeBaseName();
			int idx1 = baseName.indexOf("x264", 0, Qt::CaseInsensitive);
			int idx2 = baseName.lastIndexOf("portable", -1, Qt::CaseInsensitive);
			g_portableModeData = (idx1 >= 0) && (idx2 >= 0) && (idx1 < idx2);
		}
		g_portableModeInit = true;
	}
	
	return g_portableModeData;
}

/*
 * Get data path (i.e. path to store config files)
 */
const QString &x264_data_path(void)
{
	QReadLocker readLock(&g_dataPathLock);

	if(!g_dataPathData.isEmpty())
	{
		return g_dataPathData;
	}
	
	readLock.unlock();
	QWriteLocker writeLock(&g_dataPathLock);
	
	if(g_dataPathData.isEmpty())
	{
		g_dataPathData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
		if(g_dataPathData.isEmpty() || x264_is_portable())
		{
			g_dataPathData = QApplication::applicationDirPath();
		}
		if(!QDir(g_dataPathData).mkpath("."))
		{
			qWarning("Data directory could not be created:\n%s\n", g_dataPathData.toUtf8().constData());
			g_dataPathData = QDir::currentPath();
		}
	}
	
	return g_dataPathData;
}

/*
 * Is pre-release version?
 */
bool x264_is_prerelease(void)
{
	return (VER_X264_PRE_RELEASE);
}

/*
 * Convert path to short/ANSI path (no-op on macOS, no short path concept)
 */
QString x264_path2ansi(const QString &longPath, bool makeLowercase)
{
	Q_UNUSED(makeLowercase);
	return longPath;
}

/*
 * Inform the system that it is in use, thereby preventing the system from entering sleep
 */

#ifdef Q_OS_MAC
#include <IOKit/pwr_mgt/IOPMLib.h>
static IOPMAssertionID s_sleepAssertionId = 0;
#endif

bool x264_set_thread_execution_state(const bool systemRequired)
{
#ifdef Q_OS_MAC
	if(systemRequired)
	{
		if(s_sleepAssertionId == 0)
		{
			IOReturn ret = IOPMAssertionCreateWithName(
				kIOPMAssertionTypeNoIdleSleep,
				kIOPMAssertionLevelOn,
				CFSTR("Simple x264 Launcher encoding in progress"),
				&s_sleepAssertionId
			);
			return (ret == kIOReturnSuccess);
		}
		return true;
	}
	else
	{
		if(s_sleepAssertionId != 0)
		{
			IOPMAssertionRelease(s_sleepAssertionId);
			s_sleepAssertionId = 0;
		}
		return true;
	}
#else
	Q_UNUSED(systemRequired);
	return false;
#endif
}
