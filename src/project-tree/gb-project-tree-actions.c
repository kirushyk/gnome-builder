/* gb-project-tree-actions.c
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

#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>

#include "gb-editor-workspace.h"
#include "gb-file-manager.h"
#include "gb-project-tree.h"
#include "gb-project-tree-actions.h"
#include "gb-project-tree-private.h"
#include "gb-widget.h"
#include "gb-workbench.h"

static void
action_set (GActionGroup *group,
            const gchar  *action_name,
            const gchar  *first_param,
            ...)
{
  GAction *action;
  va_list args;

  g_assert (G_IS_ACTION_GROUP (group));
  g_assert (G_IS_ACTION_MAP (group));

  action = g_action_map_lookup_action (G_ACTION_MAP (group), action_name);
  g_assert (G_IS_SIMPLE_ACTION (action));

  va_start (args, first_param);
  g_object_set_valist (G_OBJECT (action), first_param, args);
  va_end (args);
}

static gboolean
project_file_is_directory (GObject *object)
{
  g_assert (!object || G_IS_OBJECT (object));

  return (IDE_IS_PROJECT_FILE (object) &&
          ide_project_file_get_is_directory (IDE_PROJECT_FILE (object)));
}

static void
gb_project_tree_actions_refresh (GSimpleAction *action,
                                 GVariant      *variant,
                                 gpointer       user_data)
{
  GbProjectTree *self = user_data;

  g_assert (GB_IS_PROJECT_TREE (self));

  gb_tree_rebuild (GB_TREE (self));

  /*
   * TODO:
   *
   * Try to expand back to our current position
   */
}

static void
gb_project_tree_actions_collapse_all_nodes (GSimpleAction *action,
                                            GVariant      *variant,
                                            gpointer       user_data)
{
  GbProjectTree *self = user_data;

  g_assert (GB_IS_PROJECT_TREE (self));

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (self));
}

static void
gb_project_tree_actions_open (GSimpleAction *action,
                              GVariant      *variant,
                              gpointer       user_data)
{
  GbProjectTree *self = user_data;
  GbWorkbench *workbench;
  GbTreeNode *selected;
  GObject *item;

  g_assert (GB_IS_PROJECT_TREE (self));

  workbench = gb_widget_get_workbench (GTK_WIDGET (self));
  g_assert (GB_IS_WORKBENCH (workbench));

  if (!(selected = gb_tree_get_selected (GB_TREE (self))) ||
      !(item = gb_tree_node_get_item (selected)))
    return;

  item = gb_tree_node_get_item (selected);

  if (IDE_IS_PROJECT_FILE (item))
    {
      GFileInfo *file_info;
      GFile *file;

      file_info = ide_project_file_get_file_info (IDE_PROJECT_FILE (item));
      if (!file_info)
        return;

      if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY)
        return;

      file = ide_project_file_get_file (IDE_PROJECT_FILE (item));
      if (!file)
        return;

      gb_workbench_open (workbench, file);
    }
}

static void
gb_project_tree_actions_open_with (GSimpleAction *action,
                                   GVariant      *variant,
                                   gpointer       user_data)
{
  g_autoptr(GDesktopAppInfo) app_info = NULL;
  g_autoptr(GdkAppLaunchContext) launch_context = NULL;
  GbProjectTree *self = user_data;
  GbTreeNode *selected;
  GbWorkbench *workbench;
  GdkDisplay *display;
  GFileInfo *file_info;
  GFile *file;
  const gchar *app_id;
  GObject *item;
  GList *files;

  g_assert (GB_IS_PROJECT_TREE (self));
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING));

  if (!(workbench = gb_widget_get_workbench (GTK_WIDGET (self))) ||
      !(selected = gb_tree_get_selected (GB_TREE (self))) ||
      !(item = gb_tree_node_get_item (selected)) ||
      !IDE_IS_PROJECT_FILE (item) ||
      !(app_id = g_variant_get_string (variant, NULL)) ||
      !(file_info = ide_project_file_get_file_info (IDE_PROJECT_FILE (item))) ||
      !(file = ide_project_file_get_file (IDE_PROJECT_FILE (item))) ||
      !(app_info = g_desktop_app_info_new (app_id)))
    return;

  display = gtk_widget_get_display (GTK_WIDGET (self));
  launch_context = gdk_display_get_app_launch_context (display);

  files = g_list_append (NULL, file);
  g_app_info_launch (G_APP_INFO (app_info), files, G_APP_LAUNCH_CONTEXT (launch_context), NULL);
  g_list_free (files);
}

static void
gb_project_tree_actions_open_with_editor (GSimpleAction *action,
                                          GVariant      *variant,
                                          gpointer       user_data)
{
  GbWorkbench *workbench;
  GbProjectTree *self = user_data;
  GFileInfo *file_info;
  GFile *file;
  GbTreeNode *selected;
  GObject *item;

  g_assert (GB_IS_PROJECT_TREE (self));

  if (!(selected = gb_tree_get_selected (GB_TREE (self))) ||
      !(item = gb_tree_node_get_item (selected)) ||
      !IDE_IS_PROJECT_FILE (item) ||
      !(file_info = ide_project_file_get_file_info (IDE_PROJECT_FILE (item))) ||
      (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY) ||
      !(file = ide_project_file_get_file (IDE_PROJECT_FILE (item))) ||
      !(workbench = gb_widget_get_workbench (GTK_WIDGET (self))))
    return;

  gb_workbench_open_with_editor (workbench, file);
}

static void
gb_project_tree_actions_open_containing_folder (GSimpleAction *action,
                                                GVariant      *variant,
                                                gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;
  GbTreeNode *selected;
  GObject *item;
  GFile *file;

  g_assert (GB_IS_PROJECT_TREE (self));

  if (!(selected = gb_tree_get_selected (GB_TREE (self))) ||
      !(item = gb_tree_node_get_item (selected)) ||
      !IDE_IS_PROJECT_FILE (item) ||
      !(file = ide_project_file_get_file (IDE_PROJECT_FILE (item))))
    return;

  gb_file_manager_show (file, NULL);
}

static void
gb_project_tree_actions_show_icons (GSimpleAction *action,
                                    GVariant      *variant,
                                    gpointer       user_data)
{
  GbProjectTree *self = user_data;
  gboolean show_icons;

  g_assert (GB_IS_PROJECT_TREE (self));
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_BOOLEAN));

  show_icons = g_variant_get_boolean (variant);
  gb_tree_set_show_icons (GB_TREE (self), show_icons);
  g_simple_action_set_state (action, variant);
}

static GActionEntry GbProjectTreeActions[] = {
  { "collapse-all-nodes",     gb_project_tree_actions_collapse_all_nodes },
  { "open",                   gb_project_tree_actions_open },
  { "open-containing-folder", gb_project_tree_actions_open_containing_folder },
  { "open-with",              gb_project_tree_actions_open_with, "s" },
  { "open-with-editor",       gb_project_tree_actions_open_with_editor },
  { "refresh",                gb_project_tree_actions_refresh },
  { "show-icons",             NULL, NULL, "false", gb_project_tree_actions_show_icons },
};

void
gb_project_tree_actions_init (GbProjectTree *self)
{
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GSimpleActionGroup) actions = NULL;
  g_autoptr(GAction) action = NULL;

  actions = g_simple_action_group_new ();

  settings = g_settings_new ("org.gtk.Settings.FileChooser");
  action = g_settings_create_action (settings, "sort-directories-first");
  g_action_map_add_action (G_ACTION_MAP (actions), action);

  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   GbProjectTreeActions,
                                   G_N_ELEMENTS (GbProjectTreeActions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "project-tree",
                                  G_ACTION_GROUP (actions));

  gb_project_tree_actions_update (self);
}

void
gb_project_tree_actions_update (GbProjectTree *self)
{
  GActionGroup *group;
  GbTreeNode *selection;
  GObject *item = NULL;

  IDE_ENTRY;

  g_assert (GB_IS_PROJECT_TREE (self));

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "project-tree");
  g_assert (G_IS_SIMPLE_ACTION_GROUP (group));

  selection = gb_tree_get_selected (GB_TREE (self));
  if (selection != NULL)
    item = gb_tree_node_get_item (selection);

  action_set (group, "open",
              "enabled", !project_file_is_directory (item),
              NULL);
  action_set (group, "open-with-editor",
              "enabled", !project_file_is_directory (item),
              NULL);
  action_set (group, "open-containing-folder",
              "enabled", IDE_IS_PROJECT_FILE (item),
              NULL);

  IDE_EXIT;
}