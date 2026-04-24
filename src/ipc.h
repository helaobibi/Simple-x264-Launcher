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

#pragma once

//IPC Commands
inline constexpr quint32 IPC_OPCODE_NOOP     = 0;
inline constexpr quint32 IPC_OPCODE_PING     = 1;
inline constexpr quint32 IPC_OPCODE_ADD_FILE = 2;
inline constexpr quint32 IPC_OPCODE_ADD_JOB  = 3;
inline constexpr quint32 IPC_OPCODE_MAX      = 4;

//IPC Flags
inline constexpr quint32 IPC_FLAG_FORCE_START   = 0x00000001;
inline constexpr quint32 IPC_FLAG_FORCE_ENQUEUE = 0x00000002;
