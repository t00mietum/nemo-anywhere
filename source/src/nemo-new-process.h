/* nemo-new-process: start another copy of this program to show a location.
 *
 * Copyright (c) 2026 t00mietum
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef NEMO_NEW_PROCESS_H
#define NEMO_NEW_PROCESS_H

#include <gio/gio.h>

char    **nemo_new_process_argv  (GFile   *location,
                                  GFile   *selection);
gboolean  nemo_new_process_spawn (GFile   *location,
                                  GFile   *selection,
                                  GError **error);

#endif
