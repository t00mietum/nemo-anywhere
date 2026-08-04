/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-network-win32.h - network:/// over native Windows networking.

   Copyright (C) 2026 t00mietum.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
   Boston, MA 02110-1335, USA.
*/

/* In-process network:/// scheme for Windows: the network neighborhood
 * enumerated natively (WNet), servers as folders, shares as shortcuts
 * to their UNC path, which win32 GIO handles as an ordinary native
 * location. No-op on other platforms.
 */

#ifndef NEMO_NETWORK_WIN32_H
#define NEMO_NETWORK_WIN32_H

#include <glib.h>

void nemo_network_win32_register (void);

#endif /* NEMO_NETWORK_WIN32_H */
