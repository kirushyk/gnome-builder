/* gbp-build-workbench-addin.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include "egg-binding-group.h"

#include "gbp-build-panel.h"
#include "gbp-build-workbench-addin.h"

struct _GbpBuildWorkbenchAddin
{
  GObject             parent_instance;

  /* Unowned */
  GbpBuildPanel      *panel;
  IdeWorkbench       *workbench;

  /* Owned */
  EggBindingGroup    *bindings;
  IdeBuildResult     *result;
  GSimpleActionGroup *actions;
  GCancellable       *cancellable;
  IdeDevice          *device;
};

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpBuildWorkbenchAddin, gbp_build_workbench_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN,
                                               workbench_addin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  PROP_RESULT,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
gbp_build_workbench_addin_set_device (GbpBuildWorkbenchAddin *self,
                                      IdeDevice              *device)
{
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_DEVICE (device));

  if (g_set_object (&self->device, device))
    {
      const gchar *id = ide_device_get_id (device);
      GAction *action;

      action = g_action_map_lookup_action (G_ACTION_MAP (self->actions), "device");
      g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (id));

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEVICE]);
    }
}

static void
gbp_build_workbench_addin_set_result (GbpBuildWorkbenchAddin *self,
                                      IdeBuildResult         *result)
{
  g_return_if_fail (GBP_IS_BUILD_WORKBENCH_ADDIN (self));
  g_return_if_fail (!result || IDE_IS_BUILD_RESULT (result));

  if (g_set_object (&self->result, result))
    {
      egg_binding_group_set_source (self->bindings, result);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RESULT]);
    }
}

static void
gbp_build_workbench_addin_build_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeBuilder *builder = (IdeBuilder *)object;
  g_autoptr(GbpBuildWorkbenchAddin) self = user_data;
  g_autoptr(IdeBuildResult) build_result = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUILDER (builder));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));

  build_result = ide_builder_build_finish (builder, result, &error);

  if (error != NULL)
    g_warning ("%s", error->message);
}

static void
gbp_build_workbench_addin_do_build (GbpBuildWorkbenchAddin *self,
                                    IdeBuilderBuildFlags    flags)
{
  g_autoptr(IdeBuildResult) build_result = NULL;
  g_autoptr(IdeBuilder) builder = NULL;
  g_autoptr(GError) error = NULL;
  IdeBuildSystem *build_system;
  IdeWorkbench *workbench;
  IdeContext *context;

  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));

  gbp_build_workbench_addin_set_result (self, NULL);

  workbench = ide_widget_get_workbench (GTK_WIDGET (self->panel));
  context = ide_workbench_get_context (workbench);
  build_system = ide_context_get_build_system (context);
  builder = ide_build_system_get_builder (build_system, NULL, self->device, &error);

  if (error != NULL)
    {
      gbp_build_panel_add_error (self->panel, error->message);
      return;
    }

  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  ide_builder_build_async (builder,
                           flags,
                           &build_result,
                           self->cancellable,
                           gbp_build_workbench_addin_build_cb,
                           g_object_ref (self));

  gbp_build_workbench_addin_set_result (self, build_result);
}

static void
gbp_build_workbench_addin_build (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  GbpBuildWorkbenchAddin *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));

  gbp_build_workbench_addin_do_build (self, 0);
}

static void
gbp_build_workbench_addin_rebuild (GSimpleAction *action,
                                   GVariant      *param,
                                   gpointer       user_data)
{
  GbpBuildWorkbenchAddin *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));

  gbp_build_workbench_addin_do_build (self, IDE_BUILDER_BUILD_FLAGS_FORCE_REBUILD);
}

static void
gbp_build_workbench_addin_clean (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  GbpBuildWorkbenchAddin *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));

  gbp_build_workbench_addin_do_build (self, IDE_BUILDER_BUILD_FLAGS_CLEAN);
}

static void
gbp_build_workbench_addin_cancel (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  GbpBuildWorkbenchAddin *self = user_data;

  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));

  if (self->cancellable)
    g_cancellable_cancel (self->cancellable);
}

static void
gbp_build_workbench_addin_deploy (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
}

static void
gbp_build_workbench_addin_export (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
}

static void
gbp_build_workbench_addin_device (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  GbpBuildWorkbenchAddin *self = user_data;
  IdeDeviceManager *device_manager;
  IdeContext *context;
  IdeDevice *device;
  const gchar *id;

  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  id = g_variant_get_string (param, NULL);
  if (id == NULL)
    id = "local";

  context = ide_workbench_get_context (self->workbench);
  device_manager = ide_context_get_device_manager (context);
  device = ide_device_manager_get_device (device_manager, id);

  if (device == NULL)
    device = ide_device_manager_get_device (device_manager, "local");

  gbp_build_workbench_addin_set_device (self, device);
}

static const GActionEntry actions[] = {
  { "build", gbp_build_workbench_addin_build },
  { "rebuild", gbp_build_workbench_addin_rebuild },
  { "clean", gbp_build_workbench_addin_clean },
  { "cancel", gbp_build_workbench_addin_cancel },
  { "deploy", gbp_build_workbench_addin_deploy },
  { "export", gbp_build_workbench_addin_export },
  { "device", NULL, "s", "'local'", gbp_build_workbench_addin_device },
};

static void
gbp_build_workbench_addin_load (IdeWorkbenchAddin *addin,
                                IdeWorkbench      *workbench)
{
  GbpBuildWorkbenchAddin *self = (GbpBuildWorkbenchAddin *)addin;
  IdePerspective *editor;
  GtkWidget *pane;
  IdeContext *context;
  IdeDeviceManager *device_manager;
  IdeDevice *device;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  context = ide_workbench_get_context (workbench);
  device_manager = ide_context_get_device_manager (context);
  device = ide_device_manager_get_device (device_manager, "local");

  editor = ide_workbench_get_perspective_by_name (workbench, "editor");
  pane = ide_layout_get_right_pane (IDE_LAYOUT (editor));
  self->panel = g_object_new (GBP_TYPE_BUILD_PANEL,
                              "device", device,
                              "device-manager", device_manager,
                              "visible", TRUE,
                              NULL);
  ide_layout_pane_add_page (IDE_LAYOUT_PANE (pane),
                            GTK_WIDGET (self->panel),
                            _("Build"), NULL);

  gtk_widget_insert_action_group (GTK_WIDGET (workbench), "build-tools",
                                  G_ACTION_GROUP (self->actions));

  g_object_bind_property (self, "result", self->panel, "result", 0);
  g_object_bind_property (self, "device", self->panel, "device", 0);

  gbp_build_workbench_addin_set_device (self, device);
}

static void
gbp_build_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpBuildWorkbenchAddin *self = (GbpBuildWorkbenchAddin *)addin;
  IdePerspective *editor;
  GtkWidget *pane;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  if (self->cancellable)
    g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);

  gtk_widget_insert_action_group (GTK_WIDGET (workbench), "build-tools", NULL);

  editor = ide_workbench_get_perspective_by_name (workbench, "editor");
  pane = ide_layout_get_right_pane (IDE_LAYOUT (editor));
  ide_layout_pane_remove_page (IDE_LAYOUT_PANE (pane), GTK_WIDGET (self->panel));
}

static void
gbp_build_workbench_addin_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbpBuildWorkbenchAddin *self = GBP_BUILD_WORKBENCH_ADDIN(object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    case PROP_RESULT:
      g_value_set_object (value, self->result);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_build_workbench_addin_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GbpBuildWorkbenchAddin *self = GBP_BUILD_WORKBENCH_ADDIN(object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      gbp_build_workbench_addin_set_device (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_build_workbench_addin_finalize (GObject *object)
{
  GbpBuildWorkbenchAddin *self = (GbpBuildWorkbenchAddin *)object;

  g_clear_object (&self->device);
  g_clear_object (&self->bindings);
  g_clear_object (&self->actions);
  g_clear_object (&self->result);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (gbp_build_workbench_addin_parent_class)->finalize (object);
}

static void
gbp_build_workbench_addin_class_init (GbpBuildWorkbenchAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_build_workbench_addin_finalize;
  object_class->get_property = gbp_build_workbench_addin_get_property;
  object_class->set_property = gbp_build_workbench_addin_set_property;

  properties [PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The device the build is for",
                         IDE_TYPE_DEVICE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RESULT] =
    g_param_spec_object ("result",
                         "Result",
                         "The current build result",
                         IDE_TYPE_BUILD_RESULT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gbp_build_workbench_addin_init (GbpBuildWorkbenchAddin *self)
{
  gint i;
  static const struct {
    const gchar   *property;
    const gchar   *action;
    GBindingFlags  flags;
  } bindings[] = {
    { "running", "build", G_BINDING_INVERT_BOOLEAN },
    { "running", "rebuild", G_BINDING_INVERT_BOOLEAN },
    { "running", "clean", G_BINDING_INVERT_BOOLEAN },
    { "running", "cancel", 0 },
    { "running", "deploy", G_BINDING_INVERT_BOOLEAN },
    { "running", "export", G_BINDING_INVERT_BOOLEAN },
    { NULL }
  };

  self->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   actions, G_N_ELEMENTS (actions),
                                   self);

  self->bindings = egg_binding_group_new ();

  for (i = 0; bindings [i].property; i++)
    {
      GActionMap *map = G_ACTION_MAP (self->actions);
      GAction *action;

      action = g_action_map_lookup_action (map, bindings [i].action);
      egg_binding_group_bind (self->bindings, bindings [i].property,
                              action, "enabled",
                              G_BINDING_SYNC_CREATE | bindings [i].flags);
    }
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_build_workbench_addin_load;
  iface->unload = gbp_build_workbench_addin_unload;
}
