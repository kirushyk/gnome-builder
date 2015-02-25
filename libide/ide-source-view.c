/* ide-source-view.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-source-view"

#include <glib/gi18n.h>

#include "ide-buffer.h"
#include "ide-file.h"
#include "ide-file-settings.h"
#include "ide-highlighter.h"
#include "ide-indenter.h"
#include "ide-language.h"
#include "ide-line-change-gutter-renderer.h"
#include "ide-pango.h"
#include "ide-source-view.h"

#define DEFAULT_FONT_DESC "Monospace 11"

typedef struct
{
  IdeBuffer               *buffer;
  GtkCssProvider          *css_provider;
  PangoFontDescription    *font_desc;
  IdeIndenter             *indenter;
  GtkSourceGutterRenderer *line_change_renderer;

  gulong                   buffer_changed_handler;
  gulong                   buffer_notify_file_handler;
  gulong                   buffer_notify_language_handler;

  guint                    auto_indent : 1;
  guint                    show_grid_lines : 1;
  guint                    show_line_changes : 1;
} IdeSourceViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSourceView, ide_source_view, GTK_SOURCE_TYPE_VIEW)

enum {
  PROP_0,
  PROP_AUTO_INDENT,
  PROP_FONT_NAME,
  PROP_FONT_DESC,
  PROP_SHOW_GRID_LINES,
  PROP_SHOW_LINE_CHANGES,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
ide_source_view_reload_indenter (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->auto_indent && !priv->indenter)
    gtk_source_view_set_auto_indent (GTK_SOURCE_VIEW (self), TRUE);
  else
    gtk_source_view_set_auto_indent (GTK_SOURCE_VIEW (self), FALSE);
}

static void
ide_source_view_set_indenter (IdeSourceView *self,
                              IdeIndenter   *indenter)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (!indenter || IDE_IS_INDENTER (indenter));

  if (g_set_object (&priv->indenter, indenter))
    ide_source_view_reload_indenter (self);
}

static void
ide_source_view__file_load_settings_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  g_autoptr(IdeSourceView) self = user_data;
  g_autoptr(IdeFileSettings) file_settings = NULL;
  g_autoptr(GError) error = NULL;
  IdeFile *file = (IdeFile *)object;
  IdeIndentStyle indent_style;
  guint right_margin_position;
  guint tab_width;
  gint indent_width;

  g_assert (IDE_IS_FILE (file));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  file_settings = ide_file_load_settings_finish (file, result, &error);

  if (!file_settings)
    {
      g_message ("%s", error->message);
      return;
    }

  indent_width = ide_file_settings_get_indent_width (file_settings);
  indent_style = ide_file_settings_get_indent_style (file_settings);
  right_margin_position = ide_file_settings_get_right_margin_position (file_settings);
  tab_width = ide_file_settings_get_tab_width (file_settings);

  gtk_source_view_set_indent_width (GTK_SOURCE_VIEW (self), indent_width);
  gtk_source_view_set_tab_width (GTK_SOURCE_VIEW (self), tab_width);
  gtk_source_view_set_right_margin_position (GTK_SOURCE_VIEW (self), right_margin_position);
  gtk_source_view_set_insert_spaces_instead_of_tabs (GTK_SOURCE_VIEW (self),
                                                     (indent_style == IDE_INDENT_STYLE_SPACES));
}

static void
ide_source_view_reload_file_settings (IdeSourceView *self)
{
  IdeBuffer *buffer;
  IdeFile *file;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self))));

  buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self)));
  file = ide_buffer_get_file (buffer);

  ide_file_load_settings_async (file,
                                NULL,
                                ide_source_view__file_load_settings_cb,
                                g_object_ref (self));
}

static void
ide_source_view_reload_language (IdeSourceView *self)
{
  GtkTextBuffer *buffer;
  IdeFile *file = NULL;
  IdeLanguage *language = NULL;
  GtkSourceLanguage *source_language = NULL;
  IdeIndenter *indenter;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  file = ide_buffer_get_file (IDE_BUFFER (buffer));
  language = ide_file_get_language (file);

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_FILE (file));
  g_assert (IDE_IS_LANGUAGE (language));

  source_language = ide_language_get_source_language (language);
  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), source_language);

  indenter = ide_language_get_indenter (language);
  ide_source_view_set_indenter (self, indenter);
}

static void
ide_source_view__buffer_notify_file_cb (IdeSourceView *self,
                                        GParamSpec    *pspec,
                                        IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_source_view_reload_language (self);
  ide_source_view_reload_file_settings (self);
}

static void
ide_source_view__buffer_notify_language_cb (IdeSourceView *self,
                                            GParamSpec    *pspec,
                                            IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));
}

static void
ide_source_view__buffer_changed_cb (IdeSourceView *self,
                                    IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

}

static void
ide_source_view_rebuild_css (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (!priv->css_provider)
    {
      GtkStyleContext *style_context;

      priv->css_provider = gtk_css_provider_new ();
      style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
      gtk_style_context_add_provider (style_context,
                                      GTK_STYLE_PROVIDER (priv->css_provider),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  if (priv->font_desc)
    {
      g_autofree gchar *str = NULL;
      g_autofree gchar *css = NULL;

      str = ide_pango_font_description_to_css (priv->font_desc);
      css = g_strdup_printf ("IdeSourceView { %s }", str ?: "");
      gtk_css_provider_load_from_data (priv->css_provider, css, -1, NULL);
    }
}

static void
ide_source_view_connect_buffer (IdeSourceView *self,
                                IdeBuffer     *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  priv->buffer_changed_handler =
      g_signal_connect_object (buffer,
                               "changed",
                               G_CALLBACK (ide_source_view__buffer_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

  priv->buffer_notify_file_handler =
      g_signal_connect_object (buffer,
                               "notify::file",
                               G_CALLBACK (ide_source_view__buffer_notify_file_cb),
                               self,
                               G_CONNECT_SWAPPED);

  priv->buffer_notify_language_handler =
      g_signal_connect_object (buffer,
                               "notify::language",
                               G_CALLBACK (ide_source_view__buffer_notify_language_cb),
                               self,
                               G_CONNECT_SWAPPED);

  ide_source_view__buffer_notify_language_cb (self, NULL, buffer);
  ide_source_view__buffer_notify_file_cb (self, NULL, buffer);
}

static void
ide_source_view_disconnect_buffer (IdeSourceView *self,
                                   IdeBuffer     *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_clear_signal_handler (buffer, &priv->buffer_changed_handler);
  ide_clear_signal_handler (buffer, &priv->buffer_notify_file_handler);
  ide_clear_signal_handler (buffer, &priv->buffer_notify_language_handler);

  ide_source_view_set_indenter (self, NULL);
}

static void
ide_source_view_notify_buffer (IdeSourceView *self,
                               GParamSpec    *pspec,
                               gpointer       user_data)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (priv->buffer != (IdeBuffer *)buffer)
    {
      if (priv->buffer != NULL)
        {
          ide_source_view_disconnect_buffer (self, priv->buffer);
          g_clear_object (&priv->buffer);
        }

      /*
       * Only enable IdeSourceView features if this is an IdeBuffer.
       * Ignore for GtkSourceBuffer, and GtkTextBuffer.
       */
      if (IDE_IS_BUFFER (buffer))
        {
          priv->buffer = g_object_ref (buffer);
          ide_source_view_connect_buffer (self, priv->buffer);
        }
    }
}

static gboolean
ide_source_view_key_press_event (GtkWidget   *widget,
                                 GdkEventKey *event)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  gboolean ret = FALSE;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  /*
   * Handle movement through the tab stops of the current snippet if needed.
   */
#if 0
  if ((snippet = g_queue_peek_head (priv->snippets)))
    {
      switch ((gint) event->keyval)
        {
        case GDK_KEY_Escape:
          gb_source_view_block_handlers (view);
          gb_source_view_pop_snippet (view);
          gb_source_view_scroll_to_insert (view);
          gb_source_view_unblock_handlers (view);
          return TRUE;

        case GDK_KEY_KP_Tab:
        case GDK_KEY_Tab:
          gb_source_view_block_handlers (view);
          if (!gb_source_snippet_move_next (snippet))
            gb_source_view_pop_snippet (view);
          gb_source_view_scroll_to_insert (view);
          gb_source_view_unblock_handlers (view);
          return TRUE;

        case GDK_KEY_ISO_Left_Tab:
          gb_source_view_block_handlers (view);
          gb_source_snippet_move_previous (snippet);
          gb_source_view_scroll_to_insert (view);
          gb_source_view_unblock_handlers (view);
          return TRUE;

        default:
          break;
        }
    }
#endif

  /*
   * Allow the Input Method Context to potentially filter this keystroke.
   */
  if ((event->keyval == GDK_KEY_Return) || (event->keyval == GDK_KEY_KP_Enter))
    if (gtk_text_view_im_context_filter_keypress (GTK_TEXT_VIEW (self), event))
      return TRUE;

  /*
   * If we are going to insert the same character as the next character in the
   * buffer, we may want to remove it first. This allows us to still trigger
   * the auto-indent engine (instead of just short-circuiting the key-press).
   */
#if 0
  gb_source_view_maybe_overwrite (view, event);
#endif

  /*
   * If we have an auto-indenter and the event is for a trigger key, then we
   * chain up to the parent class to insert the character, and then let the
   * auto-indenter fix things up.
   */
  if ((priv->buffer != NULL) &&
      (priv->auto_indent != FALSE) &&
      (priv->indenter != NULL) &&
      ide_indenter_is_trigger (priv->indenter, event))
    {
      GtkTextMark *insert;
      GtkTextIter begin;
      GtkTextIter end;
      gchar *indent;
      gint cursor_offset = 0;

      /*
       * Insert into the buffer so the auto-indenter can see it. If
       * GtkSourceView:auto-indent is set, then we will end up with very
       * unpredictable results.
       */
      GTK_WIDGET_CLASS (ide_source_view_parent_class)->key_press_event (widget, event);

      /*
       * Set begin and end to the position of the new insertion point.
       */
      insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (priv->buffer));
      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer), &begin, insert);
      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer), &end, insert);

      /*
       * Let the formatter potentially set the replacement text.
       */
      indent = ide_indenter_format (priv->indenter, GTK_TEXT_VIEW (self), &begin, &end,
                                    &cursor_offset, event);

      if (indent)
        {
          /*
           * Insert the indention text.
           */
          gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (priv->buffer));
          if (!gtk_text_iter_equal (&begin, &end))
            gtk_text_buffer_delete (GTK_TEXT_BUFFER (priv->buffer), &begin, &end);
          gtk_text_buffer_insert (GTK_TEXT_BUFFER (priv->buffer), &begin, indent, -1);
          g_free (indent);
          gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (priv->buffer));

          /*
           * Place the cursor, as it could be somewhere within our indent text.
           */
          gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer), &begin, insert);
          if (cursor_offset > 0)
            gtk_text_iter_forward_chars (&begin, cursor_offset);
          else if (cursor_offset < 0)
            gtk_text_iter_backward_chars (&begin, ABS (cursor_offset));
          gtk_text_buffer_select_range (GTK_TEXT_BUFFER (priv->buffer), &begin, &begin);
        }

      return TRUE;
    }

  ret = GTK_WIDGET_CLASS (ide_source_view_parent_class)->key_press_event (widget, event);

#if 0
  if (ret)
    gb_source_view_maybe_insert_match (view, event);
#endif

  return ret;
}

static void
ide_source_view_constructed (GObject *object)
{
  IdeSourceView *self = (IdeSourceView *)object;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceGutter *gutter;

  G_OBJECT_CLASS (ide_source_view_parent_class)->constructed (object);

  priv->line_change_renderer = g_object_new (IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER,
                                             "visible", priv->show_line_changes,
                                             "xpad", 1,
                                             "size", 2,
                                             NULL);
  g_object_ref (priv->line_change_renderer);
  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self), GTK_TEXT_WINDOW_LEFT);
  gtk_source_gutter_insert (gutter, priv->line_change_renderer, 0);
}

static void
ide_source_view_dispose (GObject *object)
{
  IdeSourceView *self = (IdeSourceView *)object;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_clear_object (&priv->indenter);
  g_clear_object (&priv->line_change_renderer);
  g_clear_object (&priv->css_provider);

  if (priv->buffer)
    {
      ide_source_view_disconnect_buffer (self, priv->buffer);
      g_clear_object (&priv->buffer);
    }

  g_clear_pointer (&priv->font_desc, pango_font_description_free);

  G_OBJECT_CLASS (ide_source_view_parent_class)->dispose (object);
}

static void
ide_source_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (object);
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_AUTO_INDENT:
      g_value_set_boolean (value, priv->auto_indent);
      break;

    case PROP_FONT_DESC:
      g_value_set_boxed (value, ide_source_view_get_font_desc (self));
      break;

    case PROP_SHOW_GRID_LINES:
      g_value_set_boolean (value, ide_source_view_get_show_grid_lines (self));
      break;

    case PROP_SHOW_LINE_CHANGES:
      g_value_set_boolean (value, ide_source_view_get_show_line_changes (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (object);
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_AUTO_INDENT:
      priv->auto_indent = !!g_value_get_boolean (value);
      ide_source_view_reload_indenter (self);
      break;

    case PROP_FONT_NAME:
      ide_source_view_set_font_name (self, g_value_get_string (value));
      break;

    case PROP_FONT_DESC:
      ide_source_view_set_font_desc (self, g_value_get_boxed (value));
      break;

    case PROP_SHOW_GRID_LINES:
      ide_source_view_set_show_grid_lines (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LINE_CHANGES:
      ide_source_view_set_show_line_changes (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_view_class_init (IdeSourceViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_source_view_constructed;
  object_class->dispose = ide_source_view_dispose;
  object_class->get_property = ide_source_view_get_property;
  object_class->set_property = ide_source_view_set_property;

  widget_class->key_press_event = ide_source_view_key_press_event;

  g_object_class_override_property (object_class, PROP_AUTO_INDENT, "auto-indent");

  gParamSpecs [PROP_FONT_DESC] =
    g_param_spec_boxed ("font-desc",
                        _("Font Description"),
                        _("The Pango font description to use for rendering source."),
                        PANGO_TYPE_FONT_DESCRIPTION,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FONT_DESC,
                                   gParamSpecs [PROP_FONT_DESC]);

  gParamSpecs [PROP_FONT_NAME] =
    g_param_spec_string ("font-name",
                         _("Font Name"),
                         _("The pango font name ot use for rendering source."),
                         "Monospace",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FONT_NAME,
                                   gParamSpecs [PROP_FONT_NAME]);

  gParamSpecs [PROP_SHOW_GRID_LINES] =
    g_param_spec_boolean ("show-grid-lines",
                          _("Show Grid Lines"),
                          _("If the background grid should be shown."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_GRID_LINES,
                                   gParamSpecs [PROP_SHOW_GRID_LINES]);

  gParamSpecs [PROP_SHOW_LINE_CHANGES] =
    g_param_spec_boolean ("show-line-changes",
                          _("Show Line Changes"),
                          _("If line changes should be shown in the left gutter."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_LINE_CHANGES,
                                   gParamSpecs [PROP_SHOW_LINE_CHANGES]);
}

static void
ide_source_view_init (IdeSourceView *self)
{
  g_signal_connect (self,
                    "notify::buffer",
                    G_CALLBACK (ide_source_view_notify_buffer),
                    NULL);
}

const PangoFontDescription *
ide_source_view_get_font_desc (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  return priv->font_desc;
}

void
ide_source_view_set_font_desc (IdeSourceView              *self,
                               const PangoFontDescription *font_desc)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (font_desc != priv->font_desc)
    {
      g_clear_pointer (&priv->font_desc, pango_font_description_free);

      if (font_desc)
        priv->font_desc = pango_font_description_copy (font_desc);
      else
        priv->font_desc = pango_font_description_from_string (DEFAULT_FONT_DESC);

      ide_source_view_rebuild_css (self);
    }
}

void
ide_source_view_set_font_name (IdeSourceView *self,
                               const gchar   *font_name)
{
  PangoFontDescription *font_desc = NULL;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (font_name)
    font_desc = pango_font_description_from_string (font_name);
  ide_source_view_set_font_desc (self, font_desc);
  if (font_desc)
    pango_font_description_free (font_desc);
}

gboolean
ide_source_view_get_show_line_changes (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->show_line_changes;
}

void
ide_source_view_set_show_line_changes (IdeSourceView *self,
                                       gboolean       show_line_changes)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  show_line_changes = !!show_line_changes;

  if (show_line_changes != priv->show_line_changes)
    {
      priv->show_line_changes = show_line_changes;
      if (priv->line_change_renderer)
        gtk_source_gutter_renderer_set_visible (priv->line_change_renderer, show_line_changes);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_SHOW_LINE_CHANGES]);
    }
}

gboolean
ide_source_view_get_show_grid_lines (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->show_grid_lines;
}

void
ide_source_view_set_show_grid_lines (IdeSourceView *self,
                                     gboolean       show_grid_lines)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  show_grid_lines = !!show_grid_lines;

  if (show_grid_lines != priv->show_grid_lines)
    {
      priv->show_grid_lines = show_grid_lines;
      if (show_grid_lines)
        gtk_source_view_set_background_pattern (GTK_SOURCE_VIEW (self),
                                                GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID);
      else
        gtk_source_view_set_background_pattern (GTK_SOURCE_VIEW (self),
                                                GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_SHOW_GRID_LINES]);
    }
}
