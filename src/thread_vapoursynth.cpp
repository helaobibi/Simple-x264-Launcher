///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2024 LoRd_MuldeR <MuldeR2@GMX.de>
// macOS port
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

#include "thread_vapoursynth.h"

//MUtils compat
#include "mutils_compat.h"

//Qt
#include <QEventLoop>
#include <QTimer>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

//Internal
#include "global.h"
#include "model_sysinfo.h"

//CRT
#include <cassert>

//Static
QMutex VapourSynthCheckThread::m_vpsLock;
QScopedPointer<QFile> VapourSynthCheckThread::m_vpsExePath[2];
QScopedPointer<QFile> VapourSynthCheckThread::m_vpsDllPath[2];

//Const — macOS vspipe paths to search
static const char* const VPS_PATHS[] =
{
	"/opt/homebrew/bin/vspipe",
	"/usr/local/bin/vspipe",
	"/usr/bin/vspipe",
	NULL
};

//Auxilary functions
#define BOOLIFY(X) ((X) ? '1' : '0')

//-------------------------------------
// External API
//-------------------------------------

bool VapourSynthCheckThread::detect(SysinfoModel* sysinfo)
{
	QMutexLocker lock(&m_vpsLock);

	sysinfo->clearVapourSynth();
	sysinfo->clearVPSPath();

	QEventLoop loop;
	VapourSynthCheckThread thread;

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	connect(&thread, SIGNAL(finished()), &loop, SLOT(quit()));

	thread.start();
	QTimer::singleShot(30000, &loop, SLOT(quit()));

	qDebug("VapourSynth thread has been created, please wait...");
	loop.exec(QEventLoop::ExcludeUserInputEvents);
	qDebug("VapourSynth thread finished.");

	QApplication::restoreOverrideCursor();

	if (!thread.wait(1000))
	{
		qWarning("VapourSynth thread encountered timeout -> probably deadlock!");
		thread.terminate();
		thread.wait();
		return false;
	}

	if (thread.getException())
	{
		qWarning("VapourSynth thread encountered an exception !!!");
		return false;
	}

	const int success = thread.getSuccess();
	if (!success)
	{
		qWarning("VapourSynth could not be found -> VapourSynth support disabled!");
		return true;
	}

	sysinfo->setVapourSynth(SysinfoModel::VapourSynth_X64, true);
	sysinfo->setVPSPath(thread.getPath());

	qDebug("VapourSynth support is officially enabled now!");
	return true;
}

//-------------------------------------
// Thread functions
//-------------------------------------

VapourSynthCheckThread::VapourSynthCheckThread(void)
{
	m_vpsPath[0U].clear();
	m_vpsPath[1U].clear();
}

VapourSynthCheckThread::~VapourSynthCheckThread(void)
{
}

void VapourSynthCheckThread::run(void)
{
	m_vpsPath[0U].clear();
	m_vpsPath[1U].clear();
	StarupThread::run();
}

int VapourSynthCheckThread::threadMain(void)
{
	QString vspipePath;

	// Search for vspipe in known macOS paths
	for (size_t i = 0; VPS_PATHS[i] != NULL; i++)
	{
		QFileInfo fi(VPS_PATHS[i]);
		if (fi.exists() && fi.isFile() && fi.isExecutable())
		{
			vspipePath = fi.canonicalFilePath();
			qDebug("Found vspipe at: %s", vspipePath.toUtf8().constData());
			break;
		}
	}

	// Also check PATH via QStandardPaths
	if (vspipePath.isEmpty())
	{
		const QString found = QStandardPaths::findExecutable("vspipe");
		if (!found.isEmpty())
		{
			vspipePath = found;
			qDebug("Found vspipe in PATH: %s", vspipePath.toUtf8().constData());
		}
	}

	if (vspipePath.isEmpty())
	{
		qWarning("VapourSynth (vspipe) not found -> disable VapourSynth support!");
		return 0;
	}

	// Validate by running --version
	if (checkVapourSynth(vspipePath))
	{
		m_vpsPath[0U] = QFileInfo(vspipePath).absolutePath();
		return VAPOURSYNTH_X64;
	}

	qWarning("VapourSynth was found, but version check failed!");
	return 0;
}

//-------------------------------------
// Internal functions
//-------------------------------------

bool VapourSynthCheckThread::checkVapourSynth(const QString &vspipePath)
{
	//Try to run vspipe --version
	const QStringList output = runProcess(vspipePath, QStringList() << "--version");

	//Init regular expressions
	QRegularExpression vpsLogo("VapourSynth", QRegularExpression::CaseInsensitiveOption);

	//Check for version info
	bool vapoursynthLogo = false;
	for(QStringList::ConstIterator iter = output.constBegin(); iter != output.constEnd(); iter++)
	{
		if(vpsLogo.match(*iter).hasMatch())
		{
			vapoursynthLogo = true;
			break;
		}
	}

	//Minimum required version found?
	if(vapoursynthLogo)
	{
		qDebug("VapourSynth version was detected successfully.");
		return true;
	}

	//Failed to determine version
	qWarning("Failed to determine VapourSynth version!");
	return false;
}
