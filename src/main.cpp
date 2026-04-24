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

//Internal
#include "global.h"
#include "win_main.h"
#include "cli.h"
#include "ipc.h"
#include "thread_ipc_send.h"

//MUtils compat
#include "mutils_compat.h"

//Qt includes
#include <QApplication>
#include <QDate>
#include <QStyleFactory>
#include <QFile>
#include <QTextStream>

///////////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////////

static void x264_print_logo(void)
{
	//Print version info
	qDebug("Simple x264 Launcher v%u.%02u.%u (macOS port)", x264_version_major(), x264_version_minor(), x264_version_build());
	qDebug("Copyright (c) 2004-%04d LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.", qMax(MUtils::Version::app_build_date().year(), MUtils::OS::current_date().year()));
	qDebug("Built on %s at %s with %s for %s.\n", MUTILS_UTF8(MUtils::Version::app_build_date().toString(Qt::ISODate)), MUTILS_UTF8(MUtils::Version::app_build_time().toString(Qt::ISODate)), MUTILS_UTF8(MUtils::Version::compiler_version()), MUTILS_UTF8(MUtils::Version::compiler_arch()));

	//print license info
	qDebug("This program is free software: you can redistribute it and/or modify");
	qDebug("it under the terms of the GNU General Public License <http://www.gnu.org/>.");
	qDebug("Note that this program is distributed with ABSOLUTELY NO WARRANTY.\n");

	//Print warning, if this is a "debug" build
	if(MUTILS_DEBUG)
	{
		qWarning("---------------------------------------------------------");
		qWarning("DEBUG BUILD: DO NOT RELEASE THIS BINARY TO THE PUBLIC !!!");
		qWarning("---------------------------------------------------------\n"); 
	}
}

///////////////////////////////////////////////////////////////////////////////
// Main function
///////////////////////////////////////////////////////////////////////////////

static int simple_x264_main(int &argc, char **argv)
{
	//Print logo
	x264_print_logo();

	//Get CLI arguments
	const MUtils::ArgumentMap arguments = MUtils::OS::arguments();

	//Enumerate CLI arguments
	if(!arguments.isEmpty())
	{
		qDebug("Command-Line Arguments:");
		foreach(const QString &key, arguments.uniqueKeys())
		{
			foreach(const QString &val, arguments.values(key))
			{
				if(!val.isEmpty())
				{
					qDebug("--%s = \"%s\"", MUTILS_UTF8(key), MUTILS_UTF8(val));
					continue;
				}
				qDebug("--%s", MUTILS_UTF8(key));
			}
		}
		qDebug(" ");
	}

	//Detect CPU capabilities
	const MUtils::CPUFetaures::cpu_info_t cpuFeatures = MUtils::CPUFetaures::detect();
	qDebug("   CPU vendor id  :  %s", MUTILS_UTF8(cpuFeatures.vendor));
	qDebug("CPU brand string  :  %s", MUTILS_UTF8(cpuFeatures.brand));
	qDebug("CPU architecture  :  %s", cpuFeatures.x64 ? "64-Bit" : "32-Bit");
	qDebug(" Number of CPU's  :  %d\n", cpuFeatures.count);

	//Initialize Qt
	QScopedPointer<QApplication> application(MUtils::Startup::create_qt(argc, argv, QLatin1String("Simple x264 Launcher"), QLatin1String("LoRd_MuldeR"), QLatin1String("muldersoft.com"), false));
	if(application.isNull())
	{
		return EXIT_FAILURE;
	}

	//Initialize application
	application->setWindowIcon(QIcon(":/icons/movie.ico"));
	application->setApplicationVersion(QString("%1.%2.%3").arg(QString::number(x264_version_major()), QString::number(x264_version_minor()), QString::number(x264_version_build())));

	//Initialize the IPC handler class
	QScopedPointer<MUtils::IPCChannel> ipcChannel(MUtils::IPCChannel::create("simple-x264-launcher", x264_version_build(), "instance"));

	//Running in portable mode?
	if(x264_is_portable())
	{
		qDebug("Application is running in portable mode!\n");
	}

	//Set style
	if(!arguments.contains(CLI_PARAM_NO_GUI_STYLE))
	{
		if(QStyleFactory::keys().contains("Fusion", Qt::CaseInsensitive))
		{
			qApp->setStyle(QStyleFactory::create("Fusion"));
		}
	}
	if (arguments.contains(CLI_PARAM_DARK_GUI_MODE))
	{
		QFile qss(":qdarkstyle/style.qss");
		if (qss.open(QFile::ReadOnly | QFile::Text))
		{
			QTextStream textStream(&qss);
			application->setStyleSheet(textStream.readAll());
		}
	}

	//Create Main Window
	QScopedPointer<MainWindow> mainWindow(new MainWindow(cpuFeatures, ipcChannel.data()));
	mainWindow->show();

	//Run application
	int ret = qApp->exec();
	
	//Exit program
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
// Applicaton entry point
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	return simple_x264_main(argc, argv);
}
