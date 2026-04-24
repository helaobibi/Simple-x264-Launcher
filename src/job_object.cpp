///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2024 LoRd_MuldeR <MuldeR2@GMX.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version, but always including the *additional*
// restrictions defined in the "License.txt" file.
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

#include "job_object.h"

#include <QProcess>
#include <signal.h>

JobObject::JobObject(void)
{
}

JobObject::~JobObject(void)
{
	terminateJob();
}

bool JobObject::addProcessToJob(const QProcess *proc)
{
	if(!proc)
	{
		qWarning("Cannot assign process to job: Process is NULL!");
		return false;
	}

	if(proc->state() != QProcess::Running)
	{
		qWarning("Cannot assign process to job: Process is not running!");
		return false;
	}

	qint64 pid = proc->processId();
	if(pid > 0)
	{
		m_trackedPids.append(static_cast<pid_t>(pid));
		return true;
	}
	else
	{
		qWarning("Cannot assign process to job: Process ID not available!");
		return false;
	}
}

bool JobObject::terminateJob(unsigned int exitCode)
{
	Q_UNUSED(exitCode);

	bool success = true;
	for(int i = 0; i < m_trackedPids.count(); i++)
	{
		pid_t pid = m_trackedPids[i];
		if(kill(pid, 0) == 0) // Check if process is still alive
		{
			if(kill(pid, SIGTERM) != 0)
			{
				qWarning("Failed to terminate process %d!", (int)pid);
				success = false;
			}
		}
	}
	m_trackedPids.clear();
	return success;
}
