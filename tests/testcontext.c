/* Pango
 * testcontext.c: Test program for PangoContext
 *
 * Copyright (C) 2021 Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

static void
test_list_families (void)
{
  PangoContext *context;
  PangoFontFamily **families = NULL;
  int n_families = 0;

  context = pango_context_new ();

  pango_context_list_families (context, &families, &n_families);
  g_assert_null (families);
  g_assert_cmpint (n_families, ==, 0);

  pango_context_set_font_map (context, pango_cairo_font_map_get_default ());

  pango_context_list_families (context, &families, &n_families);
  g_assert_nonnull (families);
  g_assert_cmpint (n_families, >, 0);

  g_free (families);

  g_object_unref (context);
}

static void
test_set_language (void)
{
  PangoContext *context;

  context = pango_context_new ();

  pango_context_set_language (context, pango_language_from_string ("de-de"));
  g_assert_true (pango_context_get_language (context) == pango_language_from_string ("de-de"));

  pango_context_set_language (context, NULL);
  g_assert_null (pango_context_get_language (context));

  g_object_unref (context);
}

static void
test_set_base_dir (void)
{
  PangoContext *context;

  context = pango_context_new ();

  pango_context_set_base_dir (context, PANGO_DIRECTION_RTL);
  g_assert_true (pango_context_get_base_dir (context) == PANGO_DIRECTION_RTL);

  pango_context_set_base_dir (context, PANGO_DIRECTION_WEAK_LTR);
  g_assert_true (pango_context_get_base_dir (context) == PANGO_DIRECTION_WEAK_LTR);

  g_object_unref (context);
}

static void
test_set_base_gravity (void)
{
  PangoContext *context;

  context = pango_context_new ();

  pango_context_set_base_gravity (context, PANGO_GRAVITY_SOUTH);
  g_assert_true (pango_context_get_base_gravity (context) == PANGO_GRAVITY_SOUTH);
  g_assert_true (pango_context_get_gravity (context) == PANGO_GRAVITY_SOUTH);

  pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);
  g_assert_true (pango_context_get_base_gravity (context) == PANGO_GRAVITY_AUTO);
  g_assert_true (pango_context_get_gravity (context) == PANGO_GRAVITY_SOUTH);

  g_object_unref (context);
}

static void
test_set_gravity_hint (void)
{
  PangoContext *context;

  context = pango_context_new ();

  pango_context_set_gravity_hint (context, PANGO_GRAVITY_HINT_NATURAL);
  g_assert_true (pango_context_get_gravity_hint (context) == PANGO_GRAVITY_HINT_NATURAL);

  pango_context_set_gravity_hint (context, PANGO_GRAVITY_HINT_STRONG);
  g_assert_true (pango_context_get_gravity_hint (context) == PANGO_GRAVITY_HINT_STRONG);

  g_object_unref (context);
}

static void
test_set_round_glyph_positions (void)
{
  PangoContext *context;

  context = pango_context_new ();

  pango_context_set_round_glyph_positions (context, TRUE);
  g_assert_true (pango_context_get_round_glyph_positions (context));

  pango_context_set_round_glyph_positions (context, FALSE);
  g_assert_false (pango_context_get_round_glyph_positions (context));

  g_object_unref (context);
}

static void
test_set_skip_ink (void)
{
  PangoContext *context = pango_context_new ();
  guint serial = pango_context_get_serial (context);

  g_assert_cmpint (pango_context_get_skip_ink (context), ==, PANGO_SKIP_INK_NONE);
  pango_context_set_skip_ink (context, PANGO_SKIP_INK_NONE);
  g_assert_cmpuint (pango_context_get_serial (context), ==, serial);

  for (PangoSkipInk mode = PANGO_SKIP_INK_AUTO; mode <= PANGO_SKIP_INK_ALL; mode++)
    {
      pango_context_set_skip_ink (context, mode);
      g_assert_cmpint (pango_context_get_skip_ink (context), ==, mode);
      g_assert_cmpuint (pango_context_get_serial (context), !=, serial);
      serial = pango_context_get_serial (context);
      pango_context_set_skip_ink (context, mode);
      g_assert_cmpuint (pango_context_get_serial (context), ==, serial);
    }

  g_test_expect_message ("Pango", G_LOG_LEVEL_CRITICAL, "*skip_ink*failed*");
  pango_context_set_skip_ink (context, (PangoSkipInk) -1);
  g_test_assert_expected_messages ();
  g_assert_cmpint (pango_context_get_skip_ink (context), ==, PANGO_SKIP_INK_ALL);
  g_assert_cmpuint (pango_context_get_serial (context), ==, serial);
  g_object_unref (context);
}

static void
test_skip_ink_layout (void)
{
  PangoContext *context = pango_font_map_create_context (pango_cairo_font_map_get_default ());
  PangoLayout *layout = pango_layout_new (context);
  PangoFontDescription *desc = pango_font_description_from_string ("Sans 24");
  PangoLayoutLine *line;
  guint serial;
  int width, height;

  pango_layout_set_text (layout, "AgypA", -1);
  pango_layout_get_size (layout, &width, &height);
  line = pango_layout_line_ref (pango_layout_get_line_readonly (layout, 0));
  serial = pango_layout_get_serial (layout);

  for (PangoSkipInk mode = PANGO_SKIP_INK_AUTO; mode <= PANGO_SKIP_INK_ALL; mode++)
    {
      int new_width, new_height;

      pango_context_set_skip_ink (context, mode);
      g_assert_cmpuint (pango_layout_get_serial (layout), !=, serial);
      serial = pango_layout_get_serial (layout);
      g_assert_true (pango_layout_get_line_readonly (layout, 0) == line);
      pango_layout_get_size (layout, &new_width, &new_height);
      g_assert_cmpint (new_width, ==, width);
      g_assert_cmpint (new_height, ==, height);
    }

  pango_context_set_font_description (context, desc);
  pango_context_set_skip_ink (context, PANGO_SKIP_INK_NONE);
  g_assert_cmpuint (pango_layout_get_serial (layout), !=, serial);
  g_assert_true (pango_layout_get_line_readonly (layout, 0) != line);
  pango_layout_line_unref (line);

  line = pango_layout_line_ref (pango_layout_get_line_readonly (layout, 0));
  pango_context_set_skip_ink (context, PANGO_SKIP_INK_ALL);
  pango_context_changed (context);
  g_assert_true (pango_layout_get_line_readonly (layout, 0) != line);
  pango_layout_line_unref (line);

  pango_font_description_free (desc);
  g_object_unref (layout);
  g_object_unref (context);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/context/list-families", test_list_families);
  g_test_add_func ("/context/set-language", test_set_language);
  g_test_add_func ("/context/set-base-dir", test_set_base_dir);
  g_test_add_func ("/context/set-base-gravity", test_set_base_gravity);
  g_test_add_func ("/context/set-gravity-hint", test_set_gravity_hint);
  g_test_add_func ("/context/set-round-glyph-positions", test_set_round_glyph_positions);
  g_test_add_func ("/context/set-skip-ink", test_set_skip_ink);
  g_test_add_func ("/context/skip-ink-layout", test_skip_ink_layout);

  return g_test_run ();
}
