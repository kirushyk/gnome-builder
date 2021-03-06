/* ide-omni-search-display.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_OMNI_SEARCH_DISPLAY_H
#define IDE_OMNI_SEARCH_DISPLAY_H

#include <gtk/gtk.h>

#include "ide-search-context.h"

G_BEGIN_DECLS

#define IDE_TYPE_OMNI_SEARCH_DISPLAY (ide_omni_search_display_get_type())

G_DECLARE_FINAL_TYPE (IdeOmniSearchDisplay, ide_omni_search_display, IDE, OMNI_SEARCH_DISPLAY, GtkBin)

IdeSearchContext *ide_omni_search_display_get_context          (IdeOmniSearchDisplay *self);
void              ide_omni_search_display_set_context          (IdeOmniSearchDisplay *self,
                                                                IdeSearchContext     *context);
guint64           ide_omni_search_display_get_count            (IdeOmniSearchDisplay *self);
void              ide_omni_search_display_move_next_result     (IdeOmniSearchDisplay *self);
void              ide_omni_search_display_move_previous_result (IdeOmniSearchDisplay *self);

G_END_DECLS

#endif /* IDE_OMNI_SEARCH_DISPLAY_H */
