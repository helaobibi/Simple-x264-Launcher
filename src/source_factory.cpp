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

#include "source_factory.h"

//Internal
#include "source_vapoursynth.h"

//MUtils compat
#include "mutils_compat.h"

AbstractSource *SourceFactory::createSource(const SourceType &type, JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, std::atomic<bool> *abort, std::atomic<bool> *pause, QSemaphore *semaphorePause, const QString &sourceFile)
{
	AbstractSource *source = nullptr;
	switch(type)
	{
		case SourceType_VPS: source = new VapoursynthSource(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause, sourceFile); break;
		default: MUTILS_THROW("Invalid source type!");
	}
	return source;
}

const AbstractSourceInfo& SourceFactory::getSourceInfo(const SourceType &type)
{
	const AbstractSourceInfo *sourceInfo = nullptr;
	switch(type)
	{
		case SourceType_VPS: sourceInfo = &VapoursynthSource::getSourceInfo(); break;
		default: MUTILS_THROW("Invalid source type!");
	}
	return (*sourceInfo);
}

