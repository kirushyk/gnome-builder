/* gb-search-completion.c
 *
 * Copyright (C) 2013 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gb-search-completion.h"

G_DEFINE_TYPE(GbSearchCompletion,
              gb_search_completion,
              GTK_TYPE_ENTRY_COMPLETION)

struct _GbSearchCompletionPrivate
{
   GPtrArray *providers;
};

enum
{
   COLUMN_PIXBUF,
   COLUMN_MARKUP,
   COLUMN_LAST
};

GtkEntryCompletion *
gb_search_completion_new (void)
{
   return g_object_new(GB_TYPE_SEARCH_COMPLETION, NULL);
}

void
gb_search_completion_add_provider (GbSearchCompletion *completion,
                                   GbSearchProvider   *provider)
{
   g_return_if_fail(GB_IS_SEARCH_COMPLETION(completion));
   g_return_if_fail(GB_IS_SEARCH_PROVIDER(provider));

   g_ptr_array_add(completion->priv->providers, g_object_ref(provider));
   /* TODO: sort */
   /* TODO: invalidate completions */
}

void
gb_search_completion_remove_provider (GbSearchCompletion *completion,
                                      GbSearchProvider   *provider)
{
   g_return_if_fail(GB_IS_SEARCH_COMPLETION(completion));
   g_return_if_fail(GB_IS_SEARCH_PROVIDER(provider));

   g_ptr_array_remove(completion->priv->providers, provider);
}

static void
gb_search_completion_finalize (GObject *object)
{
   G_OBJECT_CLASS(gb_search_completion_parent_class)->finalize(object);
}

static void
gb_search_completion_class_init (GbSearchCompletionClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = gb_search_completion_finalize;
   g_type_class_add_private(object_class, sizeof(GbSearchCompletionPrivate));
}

static void
gb_search_completion_init (GbSearchCompletion *completion)
{
   GtkCellRenderer *renderer;
   GtkTreeStore *model;

   completion->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(completion,
                                  GB_TYPE_SEARCH_COMPLETION,
                                  GbSearchCompletionPrivate);

   completion->priv->providers = g_ptr_array_new();
   g_ptr_array_set_free_func(completion->priv->providers, g_object_unref);

   model = gtk_tree_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);

   g_object_set(completion,
                "model", model,
                "text-column", 1,
                NULL);

   renderer = g_object_new(GTK_TYPE_CELL_RENDERER_PIXBUF,
                           "height", 16,
                           "width", 16,
                           "xpad", 3,
                           NULL);
   gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(completion), renderer, FALSE);
   gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(completion), renderer,
                                 "pixbuf", COLUMN_PIXBUF);

   renderer = g_object_new(GTK_TYPE_CELL_RENDERER_TEXT,
                           "xalign", 0.0f,
                           NULL);
   gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(completion), renderer, TRUE);
   gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(completion), renderer,
                                 "markup", COLUMN_MARKUP);
}
