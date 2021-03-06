/* gb-device-manager-panel.c
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
#include <libpeas/peas.h>

#include "gb-device-manager-panel.h"
#include "gb-device-manager-tree-builder.h"
#include "gb-tree.h"
#include "gb-workspace.h"

struct _GbDeviceManagerPanel
{
  GtkBox  parent_instance;
  GbTree *tree;
};

static void workbench_addin_iface_init (GbWorkbenchAddinInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GbDeviceManagerPanel,
                                gb_device_manager_panel,
                                GTK_TYPE_BOX,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (GB_TYPE_WORKBENCH_ADDIN,
                                                               workbench_addin_iface_init))


static void
gb_device_manager_panel_load (GbWorkbenchAddin *addin,
                              GbWorkbench      *workbench)
{
  GbDeviceManagerPanel *self = (GbDeviceManagerPanel *)addin;
  IdeContext *context;
  IdeDeviceManager *device_manager;
  GbTreeNode *root;
  GtkWidget *workspace;
  GtkWidget *pane;

  g_return_if_fail (GB_IS_DEVICE_MANAGER_PANEL (self));

  context = gb_workbench_get_context (workbench);
  device_manager = ide_context_get_device_manager (context);

  root = gb_tree_node_new ();
  gb_tree_node_set_item (root, G_OBJECT (device_manager));
  gb_tree_set_root (self->tree, root);

  workspace = gb_workbench_get_workspace (workbench);
  pane = gb_workspace_get_left_pane (GB_WORKSPACE (workspace));
  gb_workspace_pane_add_page (GB_WORKSPACE_PANE (pane),
                              GTK_WIDGET (self),
                              _("Device Manager"),
                              "computer-symbolic");
}

static void
gb_device_manager_panel_unload (GbWorkbenchAddin *addin,
                                GbWorkbench      *workbench)
{
  /*
   * TODO: We shouldn't add/remove ourselves. Instead, we should make
   *       the addin structure a separate object which adds this panel.
   */
}

static void
gb_device_manager_panel_class_init (GbDeviceManagerPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/device-manager/gb-device-manager-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbDeviceManagerPanel, tree);

  g_type_ensure (GB_TYPE_DEVICE_MANAGER_TREE_BUILDER);
}

static void
gb_device_manager_panel_class_finalize (GbDeviceManagerPanelClass *klass)
{
}

static void
gb_device_manager_panel_init (GbDeviceManagerPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
workbench_addin_iface_init (GbWorkbenchAddinInterface *iface)
{
  iface->load = gb_device_manager_panel_load;
  iface->unload = gb_device_manager_panel_unload;
}

void
peas_register_types (PeasObjectModule *module)
{
  gb_device_manager_panel_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              GB_TYPE_WORKBENCH_ADDIN,
                                              GB_TYPE_DEVICE_MANAGER_PANEL);
}
