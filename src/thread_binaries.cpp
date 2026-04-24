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

#include "thread_binaries.h"

#include <memory>
#include <QLibrary>
#include <QEventLoop>
#include <QTimer>
#include <QSet>
#include <QMutexLocker>
#include <QApplication>
#include <QProcess>
#include <QDir>
#include <QFileInfo>

//Internal
#include "global.h"
#include "model_sysinfo.h"
#include "encoder_factory.h"
#include "source_factory.h"

//MUtils compat
#include "mutils_compat.h"

//Static
QMutex BinariesCheckThread::m_binLock;
QScopedPointer<QFile> BinariesCheckThread::m_binPath[MAX_BINARIES];

//Whatever
#define NEXT(X) ((*reinterpret_cast<int*>(&(X)))++)
#define BOOLIFY(X) (!!(X))

//-------------------------------------
// External API
//-------------------------------------

bool BinariesCheckThread::check(const SysinfoModel *const sysinfo, QString *const failedPath)
{
	QMutexLocker lock(&m_binLock);

	QEventLoop loop;
	BinariesCheckThread thread(sysinfo);

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	connect(&thread, SIGNAL(finished()), &loop, SLOT(quit()));
	
	thread.start();
	QTimer::singleShot(30000, &loop, SLOT(quit()));
	
	qDebug("Binaries checker thread has been created, please wait...");
	loop.exec(QEventLoop::ExcludeUserInputEvents);
	qDebug("Binaries checker thread finished.");

	QApplication::restoreOverrideCursor();

	if(!thread.wait(5000))
	{
		qWarning("Binaries checker thread encountered timeout -> probably deadlock!");
		thread.terminate();
		thread.wait();
		return false;
	}

	if(thread.getException())
	{
		qWarning("Binaries checker thread encountered an exception !!!");
		return false;
	}
	
	const bool success = BOOLIFY(thread.getSuccess());
	if ((!success) && failedPath)
	{
		*failedPath = thread.getFailedPath();
	}

	return success;
}

//-------------------------------------
// Thread class
//-------------------------------------

BinariesCheckThread::BinariesCheckThread(const SysinfoModel *const sysinfo)
:
	m_sysinfo(sysinfo)
{
	m_failedPath.clear();
}

BinariesCheckThread::~BinariesCheckThread(void)
{
}

void BinariesCheckThread::run(void)
{
	m_failedPath.clear();
	StartupThread::run();
}

int BinariesCheckThread::threadMain(void)
{
	//Create list of all required binary files
	QStringList binFiles;
	QSet<QString> filesSet;

	for(OptionsModel::EncType encdr = OptionsModel::EncType_MIN; encdr <= OptionsModel::EncType_MAX; NEXT(encdr))
	{
		const AbstractEncoderInfo &encInfo = EncoderFactory::getEncoderInfo(encdr);
		const quint32 archCount = encInfo.getArchitectures().count();
		for (quint32 archIdx = 0; archIdx < archCount; ++archIdx)
		{
			const QStringList variants = encInfo.getVariants();
			for (quint32 varntIdx = 0; varntIdx < quint32(variants.count()); ++varntIdx)
			{
				const QString binary = encInfo.getBinaryPath(m_sysinfo, archIdx, varntIdx);
				if (!filesSet.contains(binary))
				{
					filesSet << binary;
					binFiles << binary;
				}
			}
		}
	}

	//Actually validate the binaries - just check they exist and are executable on macOS
	size_t currentFile = 0;
	for(QStringList::ConstIterator iter = binFiles.constBegin(); iter != binFiles.constEnd(); iter++)
	{
		QFileInfo fi(*iter);
		qDebug("%s", MUTILS_UTF8(*iter));

		if(fi.exists() && fi.isFile() && fi.isExecutable())
		{
			std::unique_ptr<QFile> file(new QFile(*iter));
			if(file->open(QIODevice::ReadOnly))
			{
				if(currentFile < MAX_BINARIES)
				{
					m_binPath[currentFile++].reset(file.release());
					continue;
				}
				qFatal("Current binary file exceeds max. number of binaries!");
			}
		}

		m_failedPath = *iter;
		qWarning("Required tool could not be found or is not executable:\n%s\n", MUTILS_UTF8(*iter));
		return 0;
	}

	return 1;
}
