/* test-skip-ink.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <pango/pango.h>

#define UNIT PANGO_SCALE

typedef struct
{
  PangoFont parent_instance;
  unsigned int draws;
  int position;
} InkFont;

typedef PangoFontClass InkFontClass;

GType ink_font_get_type (void);
G_DEFINE_TYPE (InkFont, ink_font, PANGO_TYPE_FONT)

static void
font_draw (hb_font_t       *font,
           void            *font_data,
           hb_codepoint_t   glyph,
           hb_draw_funcs_t *funcs,
           void            *data,
           void            *user_data)
{
  InkFont *self = font_data;
  hb_draw_state_t state = HB_DRAW_STATE_DEFAULT;

  self->draws++;

  if (glyph == 1)
    {
      hb_draw_move_to (funcs, data, &state, 4 * UNIT, 10 * UNIT);
      hb_draw_line_to (funcs, data, &state, 6 * UNIT, 10 * UNIT);
      hb_draw_line_to (funcs, data, &state, 6 * UNIT, -5 * UNIT);
      hb_draw_line_to (funcs, data, &state, 4 * UNIT, -5 * UNIT);
    }
  else if (glyph == 2)
    {
      hb_draw_move_to (funcs, data, &state, 0, 0);
      hb_draw_quadratic_to (funcs, data, &state, 5 * UNIT, -20 * UNIT, 10 * UNIT, 0);
    }
  else if (glyph == 3)
    {
      hb_draw_move_to (funcs, data, &state, 0, 0);
      hb_draw_cubic_to (funcs, data, &state, 0, -20 * UNIT,
                        10 * UNIT, -20 * UNIT, 10 * UNIT, 0);
    }
  else
    return;

  hb_draw_close_path (funcs, data, &state);
}

static hb_font_t *
font_create_hb_font (PangoFont *font)
{
  hb_font_t *hb_font = hb_font_create (hb_face_get_empty ());
  hb_font_funcs_t *funcs = hb_font_funcs_create ();

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  hb_font_funcs_set_draw_glyph_func (funcs, font_draw, NULL, NULL);
  G_GNUC_END_IGNORE_DEPRECATIONS
  hb_font_set_funcs (hb_font, funcs, font, NULL);
  hb_font_funcs_destroy (funcs);

  return hb_font;
}

static void
font_get_extents (PangoFont      *font,
                  PangoGlyph      glyph,
                  PangoRectangle *ink,
                  PangoRectangle *logical)
{
  if (ink)
    *ink = (PangoRectangle) { 4 * UNIT, -10 * UNIT, 2 * UNIT, 15 * UNIT };
  if (logical)
    *logical = (PangoRectangle) { 0, -10 * UNIT, 10 * UNIT, 15 * UNIT };
}

static PangoFontMetrics *
font_get_metrics (PangoFont     *font,
                  PangoLanguage *language)
{
  InkFont *self = (InkFont *) font;
  PangoFontMetrics *metrics = g_slice_new0 (PangoFontMetrics);

  metrics->ref_count = 1;
  metrics->ascent = 10 * UNIT;
  metrics->descent = 5 * UNIT;
  metrics->underline_position = self->position;
  metrics->underline_thickness = UNIT;
  metrics->strikethrough_position = 3 * UNIT;
  metrics->strikethrough_thickness = UNIT;

  return metrics;
}

static void
ink_font_init (InkFont *self)
{
  self->position = -UNIT;
}

static void
ink_font_class_init (InkFontClass *klass)
{
  klass->create_hb_font = font_create_hb_font;
  klass->get_glyph_extents = font_get_extents;
  klass->get_metrics = font_get_metrics;
}

typedef struct
{
  PangoRenderPart part;
  PangoRectangle rect;
} Rectangle;

typedef struct
{
  PangoRenderer parent_instance;
  GArray *rectangles;
  unsigned int errors;
} InkRenderer;

typedef PangoRendererClass InkRendererClass;

GType ink_renderer_get_type (void);
G_DEFINE_TYPE (InkRenderer, ink_renderer, PANGO_TYPE_RENDERER)

static void
renderer_rectangle (PangoRenderer  *renderer,
                    PangoRenderPart part,
                    int             x,
                    int             y,
                    int             width,
                    int             height)
{
  InkRenderer *self = (InkRenderer *) renderer;
  Rectangle rect = { part, { x, y, width, height } };

  g_array_append_val (self->rectangles, rect);
}

static void
renderer_error (PangoRenderer *renderer,
                int            x,
                int            y,
                int            width,
                int            height)
{
  InkRenderer *self = (InkRenderer *) renderer;

  self->errors++;
}

static void
renderer_finalize (GObject *object)
{
  InkRenderer *self = (InkRenderer *) object;

  g_clear_pointer (&self->rectangles, g_array_unref);

  G_OBJECT_CLASS (ink_renderer_parent_class)->finalize (object);
}

static void
ink_renderer_init (InkRenderer *self)
{
  self->rectangles = g_array_new (FALSE, FALSE, sizeof (Rectangle));
  pango_renderer_set_components (PANGO_RENDERER (self),
                                 PANGO_RENDER_COMPONENT_UNDERLINE |
                                 PANGO_RENDER_COMPONENT_OVERLINE |
                                 PANGO_RENDER_COMPONENT_STRIKETHROUGH);
}

static void
ink_renderer_class_init (InkRendererClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = renderer_finalize;

  klass->draw_rectangle = renderer_rectangle;
  klass->draw_error_underline = renderer_error;
}

static PangoGlyphItem *
make_run (PangoFont      *font,
          PangoUnderline  underline)
{
  PangoGlyphItem *run = g_new0 (PangoGlyphItem, 1);

  run->item = pango_item_new ();
  run->item->analysis.font = g_object_ref (font);
  run->item->analysis.script = PANGO_SCRIPT_LATIN;
  run->item->analysis.extra_attrs = g_slist_append (NULL, pango_attr_underline_new (underline));
  run->glyphs = pango_glyph_string_new ();
  pango_glyph_string_set_size (run->glyphs, 1);
  run->glyphs->glyphs[0] = (PangoGlyphInfo) { .glyph = 1, .geometry.width = 10 * UNIT };
  run->glyphs->log_clusters[0] = 0;

  return run;
}

static void
assert_rectangle (InkRenderer *renderer,
                  unsigned int index,
                  int          x,
                  int          y,
                  int          width)
{
  Rectangle *rect = &g_array_index (renderer->rectangles, Rectangle, index);

  g_assert_cmpint (rect->rect.x, ==, x * UNIT);
  g_assert_cmpint (rect->rect.y, ==, y * UNIT);
  g_assert_cmpint (rect->rect.width, ==, width * UNIT);
  g_assert_cmpint (rect->rect.height, ==, UNIT);
}

static void
test_intersections (void)
{
  const struct
  {
    PangoGlyph glyph;
    int position;
    gboolean intersects;
  } cases[] = {
    { 1, 1, TRUE },
    { 1, 6, FALSE },
    { 0, 1, FALSE },
    { PANGO_GLYPH_EMPTY, 1, FALSE },
    { PANGO_GET_UNKNOWN_GLYPH ('x'), 1, FALSE },
    /* The control points reach these bands, but the curves do not. */
    { 2, 11, FALSE },
    { 3, 16, FALSE },
    { 2, 9, TRUE },
    { 3, 14, TRUE },
  };
  InkFont *font = g_object_new (ink_font_get_type (), NULL);
  InkRenderer *renderer = g_object_new (ink_renderer_get_type (), NULL);
  PangoGlyphItem *run = make_run (PANGO_FONT (font), PANGO_UNDERLINE_SINGLE);
  PangoContext *context = pango_context_new ();
  PangoLayout *layout = pango_layout_new (context);
  PangoLayoutLine line = { .layout = layout, .runs = g_slist_append (NULL, run) };

  pango_context_set_skip_ink (context, PANGO_SKIP_INK_ALL);

  for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      g_array_set_size (renderer->rectangles, 0);
      font->position = -cases[i].position * UNIT;
      run->glyphs->glyphs[0].glyph = cases[i].glyph;
      pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
      g_assert_cmpuint (renderer->rectangles->len, ==, cases[i].intersects ? 2 : 1);

      if (!cases[i].intersects)
        assert_rectangle (renderer, 0, 0, cases[i].position, 10);
      else if (cases[i].glyph == 1)
        {
          assert_rectangle (renderer, 0, 0, cases[i].position, 3);
          assert_rectangle (renderer, 1, 7, cases[i].position, 3);
        }
      else
        {
          Rectangle *left = &g_array_index (renderer->rectangles, Rectangle, 0);
          Rectangle *right = &g_array_index (renderer->rectangles, Rectangle, 1);

          g_assert_cmpint (left->rect.x, ==, 0);
          g_assert_cmpint (left->rect.width, >, 2 * UNIT);
          g_assert_cmpint (left->rect.width, <, 5 * UNIT);
          g_assert_cmpint (right->rect.x, >, 5 * UNIT);
          g_assert_cmpint (right->rect.x, <, 8 * UNIT);
          g_assert_cmpint (right->rect.x + right->rect.width, ==, 10 * UNIT);
        }
    }

  g_object_unref (layout);
  g_object_unref (context);
  g_slist_free_full (line.runs, (GDestroyNotify) pango_glyph_item_free);
  g_object_unref (renderer);
  g_object_unref (font);
}

static void
test_cache (void)
{
  InkFont *font = g_object_new (ink_font_get_type (), NULL);
  InkRenderer *renderer = g_object_new (ink_renderer_get_type (), NULL);
  PangoGlyphItem *run = make_run (PANGO_FONT (font), PANGO_UNDERLINE_SINGLE);
  PangoContext *context = pango_context_new ();
  PangoLayout *layout = pango_layout_new (context);
  PangoLayoutLine line = { .layout = layout, .runs = g_slist_append (NULL, run) };
  unsigned int draws = 0;

  pango_context_set_skip_ink (context, PANGO_SKIP_INK_ALL);

  for (int i = 0; i < 2; i++)
    {
      g_array_set_size (renderer->rectangles, 0);
      font->position = -UNIT;
      pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
      g_assert_cmpuint (renderer->rectangles->len, ==, 2);
      assert_rectangle (renderer, 0, 0, 1, 3);
      assert_rectangle (renderer, 1, 7, 1, 3);

      g_array_set_size (renderer->rectangles, 0);
      font->position = -6 * UNIT;
      pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
      g_assert_cmpuint (renderer->rectangles->len, ==, 1);
      assert_rectangle (renderer, 0, 0, 6, 10);

      if (i == 0)
        {
          draws = font->draws;
          g_assert_cmpuint (draws, >, 0);
        }
      else
        g_assert_cmpuint (font->draws, ==, draws);
    }

  g_object_unref (layout);
  g_object_unref (context);
  g_slist_free_full (line.runs, (GDestroyNotify) pango_glyph_item_free);
  g_object_unref (renderer);
  g_object_unref (font);
}

static void
test_renderer (void)
{
  PangoFont *font = g_object_new (ink_font_get_type (), NULL);
  InkRenderer *renderer = g_object_new (ink_renderer_get_type (), NULL);
  PangoGlyphItem *run = make_run (font, PANGO_UNDERLINE_SINGLE);
  PangoContext *context = pango_context_new ();
  PangoContext *other_context = pango_context_new ();
  PangoLayout *other_layout = pango_layout_new (other_context);
  PangoLayout *layout = pango_layout_new (context);
  PangoLayoutLine line = { .layout = layout, .runs = g_slist_append (NULL, run) };

  g_assert_cmpint (pango_context_get_skip_ink (context), ==, PANGO_SKIP_INK_NONE);
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 1);
  assert_rectangle (renderer, 0, 0, 1, 10);
  g_assert_cmpuint (((InkFont *) font)->draws, ==, 0);

  g_array_set_size (renderer->rectangles, 0);
  pango_context_set_skip_ink (context, PANGO_SKIP_INK_AUTO);
  g_assert_cmpint (pango_context_get_skip_ink (context), ==, PANGO_SKIP_INK_AUTO);
  pango_renderer_set_components (PANGO_RENDERER (renderer), PANGO_RENDER_COMPONENT_NONE);
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 0);
  g_assert_cmpuint (((InkFont *) font)->draws, ==, 0);
  pango_renderer_set_components (PANGO_RENDERER (renderer), PANGO_RENDER_COMPONENT_UNDERLINE);
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 2);
  assert_rectangle (renderer, 0, 0, 1, 3);
  assert_rectangle (renderer, 1, 7, 1, 3);

  g_array_set_size (renderer->rectangles, 0);
  run->glyphs->glyphs[0].geometry.x_offset = UNIT;
  run->glyphs->glyphs[0].geometry.y_offset = 10 * UNIT;
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 20 * UNIT, 30 * UNIT);
  g_assert_cmpuint (renderer->rectangles->len, ==, 2);
  assert_rectangle (renderer, 0, 20, 31, 4);
  assert_rectangle (renderer, 1, 28, 31, 2);

  for (guint i = 0; i < 4; i++)
    {
      g_array_set_size (renderer->rectangles, 0);
      line.layout = i % 2 ? layout : other_layout;
      pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
      g_assert_cmpuint (renderer->rectangles->len, ==, i % 2 ? 2 : 1);
    }

  g_object_unref (other_layout);
  g_object_unref (other_context);
  g_object_unref (layout);
  g_object_unref (context);
  g_slist_free_full (line.runs, (GDestroyNotify) pango_glyph_item_free);
  g_object_unref (renderer);
  g_object_unref (font);
}

static void
test_merged_runs (void)
{
  PangoFont *font = g_object_new (ink_font_get_type (), NULL);
  InkFont *lower_font = g_object_new (ink_font_get_type (), NULL);
  InkRenderer *renderer = g_object_new (ink_renderer_get_type (), NULL);
  PangoGlyphItem *first = make_run (font, PANGO_UNDERLINE_SINGLE_LINE);
  PangoGlyphItem *second = make_run (PANGO_FONT (lower_font), PANGO_UNDERLINE_SINGLE_LINE);
  PangoContext *context = pango_context_new ();
  PangoLayout *layout = pango_layout_new (context);
  PangoLayoutLine line = { .layout = layout };

  lower_font->position = -6 * UNIT;
  line.runs = g_slist_append (line.runs, first);
  line.runs = g_slist_append (line.runs, second);
  pango_context_set_skip_ink (context, PANGO_SKIP_INK_AUTO);
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 1);
  assert_rectangle (renderer, 0, 0, 6, 20);

  g_array_set_size (renderer->rectangles, 0);
  lower_font->position = -UNIT;
  second->glyphs->glyphs[0].geometry.x_offset = -10 * UNIT;
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 2);
  assert_rectangle (renderer, 0, 0, 1, 3);
  assert_rectangle (renderer, 1, 7, 1, 7);

  g_array_set_size (renderer->rectangles, 0);
  second->glyphs->glyphs[0].geometry.x_offset = 0;
  second->item->analysis.script = PANGO_SCRIPT_HAN;
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 2);
  assert_rectangle (renderer, 0, 0, 1, 3);
  assert_rectangle (renderer, 1, 7, 1, 13);

  g_array_set_size (renderer->rectangles, 0);
  pango_context_set_skip_ink (context, PANGO_SKIP_INK_ALL);
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 3);
  assert_rectangle (renderer, 0, 0, 1, 3);
  assert_rectangle (renderer, 1, 7, 1, 6);
  assert_rectangle (renderer, 2, 17, 1, 3);

  g_object_unref (layout);
  g_object_unref (context);
  g_slist_free_full (line.runs, (GDestroyNotify) pango_glyph_item_free);
  g_object_unref (renderer);
  g_object_unref (font);
  g_object_unref (lower_font);
}

static void
test_gap_order (void)
{
  const int positions[][4] = {
    { 0, 10, 20, 30 },
    { 30, 20, 10, 0 },
    { 20, 0, 30, 10 },
    { 0, 0, 0, 0 },
    { 0, 2, 4, 6 },
    { 6, 2, 4, 0 },
    { 0, 10, 2, 12 },
  };
  PangoFont *font = g_object_new (ink_font_get_type (), NULL);
  InkRenderer *renderer = g_object_new (ink_renderer_get_type (), NULL);
  PangoGlyphItem *run = make_run (font, PANGO_UNDERLINE_SINGLE);
  PangoContext *context = pango_context_new ();
  PangoLayout *layout = pango_layout_new (context);
  PangoLayoutLine line = { .layout = layout, .runs = g_slist_append (NULL, run) };

  pango_context_set_skip_ink (context, PANGO_SKIP_INK_ALL);
  pango_glyph_string_set_size (run->glyphs, 4);

  for (guint i = 0; i < G_N_ELEMENTS (positions); i++)
    {
      gboolean skipped[40] = { FALSE, };
      guint n_rectangles = 0;

      for (int j = 0; j < 4; j++)
        {
          run->glyphs->glyphs[j] = (PangoGlyphInfo) {
            .glyph = 1,
            .geometry.width = 10 * UNIT,
            .geometry.x_offset = (positions[i][j] - j * 10) * UNIT,
          };
          run->glyphs->log_clusters[j] = j;

          for (int x = positions[i][j] + 3; x < positions[i][j] + 7; x++)
            skipped[x] = TRUE;
        }

      g_array_set_size (renderer->rectangles, 0);
      pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);

      for (int x = 0; x < 40;)
        {
          int start = x;

          if (skipped[x])
            {
              x++;
              continue;
            }

          while (x < 40 && !skipped[x])
            x++;

          g_assert_cmpuint (n_rectangles, <, renderer->rectangles->len);
          assert_rectangle (renderer, n_rectangles++, start, 1, x - start);
        }

      g_assert_cmpuint (renderer->rectangles->len, ==, n_rectangles);
    }

  g_object_unref (layout);
  g_object_unref (context);
  g_slist_free_full (line.runs, (GDestroyNotify) pango_glyph_item_free);
  g_object_unref (renderer);
  g_object_unref (font);
}

static void
test_centered_baseline (void)
{
  PangoFont *font = g_object_new (ink_font_get_type (), NULL);
  InkRenderer *renderer = g_object_new (ink_renderer_get_type (), NULL);
  PangoGlyphItem *run = make_run (font, PANGO_UNDERLINE_SINGLE);
  PangoContext *context = pango_context_new ();
  PangoLayout *layout = pango_layout_new (context);
  PangoLayoutLine line = { .layout = layout, .runs = g_slist_append (NULL, run) };

  run->item->analysis.flags |= PANGO_ANALYSIS_FLAG_CENTERED_BASELINE;
  run->start_x_offset = 2 * UNIT;
  run->end_x_offset = 3 * UNIT;
  pango_context_set_skip_ink (context, PANGO_SKIP_INK_ALL);
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);

  g_assert_cmpuint (renderer->rectangles->len, ==, 2);
  assert_rectangle (renderer, 0, 2, 3, 3);
  assert_rectangle (renderer, 1, 9, 3, 3);

  g_object_unref (layout);
  g_object_unref (context);
  g_slist_free_full (line.runs, (GDestroyNotify) pango_glyph_item_free);
  g_object_unref (renderer);
  g_object_unref (font);
}

static void
test_exclusions (void)
{
  PangoFont *font = g_object_new (ink_font_get_type (), NULL);
  InkRenderer *renderer = g_object_new (ink_renderer_get_type (), NULL);
  PangoGlyphItem *run = make_run (font, PANGO_UNDERLINE_SINGLE);
  PangoContext *context = pango_context_new ();
  PangoLayout *layout = pango_layout_new (context);
  PangoLayoutLine line = { .layout = layout, .runs = g_slist_append (NULL, run) };

  const PangoScript scripts[] = {
    PANGO_SCRIPT_LATIN, PANGO_SCRIPT_HAN, PANGO_SCRIPT_HANGUL,
    PANGO_SCRIPT_HIRAGANA, PANGO_SCRIPT_KATAKANA, PANGO_SCRIPT_BOPOMOFO
  };

  for (guint i = 0; i < G_N_ELEMENTS (scripts); i++)
    for (PangoSkipInk mode = PANGO_SKIP_INK_NONE; mode <= PANGO_SKIP_INK_ALL; mode++)
      {
        gboolean gaps = mode == PANGO_SKIP_INK_ALL ||
                        (mode == PANGO_SKIP_INK_AUTO && scripts[i] == PANGO_SCRIPT_LATIN);

        g_array_set_size (renderer->rectangles, 0);
        run->item->analysis.script = scripts[i];
        pango_context_set_skip_ink (context, mode);
        pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
        g_assert_cmpuint (renderer->rectangles->len, ==, gaps ? 2 : 1);
      }

  g_array_set_size (renderer->rectangles, 0);
  line.layout = NULL;
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 1);
  line.layout = layout;

  g_array_set_size (renderer->rectangles, 0);
  run->item->analysis.gravity = PANGO_GRAVITY_EAST;
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 1);
  run->item->analysis.gravity = PANGO_GRAVITY_SOUTH;

  g_array_set_size (renderer->rectangles, 0);
  run->item->analysis.script = PANGO_SCRIPT_LATIN;
  run->glyphs->glyphs[0].attr.is_color = TRUE;
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 1);

  g_array_set_size (renderer->rectangles, 0);
  run->glyphs->glyphs[0].attr.is_color = FALSE;
  ((PangoAttrInt *) run->item->analysis.extra_attrs->data)->value = PANGO_UNDERLINE_ERROR;
  run->item->analysis.extra_attrs = g_slist_append (run->item->analysis.extra_attrs,
                                                    pango_attr_strikethrough_new (TRUE));
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->errors, ==, 1);
  g_assert_cmpuint (renderer->rectangles->len, ==, 1);
  g_assert_cmpint (g_array_index (renderer->rectangles, Rectangle, 0).part, ==, PANGO_RENDER_PART_STRIKETHROUGH);


  g_object_unref (layout);
  g_object_unref (context);
  g_slist_free_full (line.runs, (GDestroyNotify) pango_glyph_item_free);
  g_object_unref (renderer);
  g_object_unref (font);
}

static void
test_decorations (void)
{
  PangoFont *font = g_object_new (ink_font_get_type (), NULL);
  InkRenderer *renderer = g_object_new (ink_renderer_get_type (), NULL);
  PangoGlyphItem *run = make_run (font, PANGO_UNDERLINE_DOUBLE);
  PangoContext *context = pango_context_new ();
  PangoLayout *layout = pango_layout_new (context);
  PangoLayoutLine line = { .layout = layout, .runs = g_slist_append (NULL, run) };

  pango_context_set_skip_ink (context, PANGO_SKIP_INK_AUTO);
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 4);
  assert_rectangle (renderer, 0, 0, 3, 3);
  assert_rectangle (renderer, 1, 7, 3, 3);
  assert_rectangle (renderer, 2, 0, 1, 3);
  assert_rectangle (renderer, 3, 7, 1, 3);

  g_array_set_size (renderer->rectangles, 0);
  ((PangoAttrInt *) run->item->analysis.extra_attrs->data)->value = PANGO_UNDERLINE_NONE;
  run->item->analysis.extra_attrs = g_slist_append (run->item->analysis.extra_attrs,
                                                    pango_attr_overline_new (PANGO_OVERLINE_SINGLE));
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 2);
  assert_rectangle (renderer, 0, 0, -10, 3);
  assert_rectangle (renderer, 1, 7, -10, 3);

  g_object_unref (layout);
  g_object_unref (context);
  g_slist_free_full (line.runs, (GDestroyNotify) pango_glyph_item_free);
  g_object_unref (renderer);
  g_object_unref (font);
}

static void
test_color_boundary (void)
{
  PangoFont *font = g_object_new (ink_font_get_type (), NULL);
  InkRenderer *renderer = g_object_new (ink_renderer_get_type (), NULL);
  PangoGlyphItem *first = make_run (font, PANGO_UNDERLINE_SINGLE);
  PangoGlyphItem *second = make_run (font, PANGO_UNDERLINE_SINGLE);
  PangoContext *context = pango_context_new ();
  PangoLayout *layout = pango_layout_new (context);
  PangoLayoutLine line = { .layout = layout };

  second->item->analysis.extra_attrs = g_slist_append (second->item->analysis.extra_attrs,
                                                       pango_attr_underline_color_new (65535, 0, 0));
  line.runs = g_slist_append (line.runs, first);
  line.runs = g_slist_append (line.runs, second);
  pango_context_set_skip_ink (context, PANGO_SKIP_INK_AUTO);
  pango_renderer_draw_layout_line (PANGO_RENDERER (renderer), &line, 0, 0);
  g_assert_cmpuint (renderer->rectangles->len, ==, 4);
  assert_rectangle (renderer, 0, 0, 1, 3);
  assert_rectangle (renderer, 1, 7, 1, 3);
  assert_rectangle (renderer, 2, 10, 1, 3);
  assert_rectangle (renderer, 3, 17, 1, 3);

  g_object_unref (layout);
  g_object_unref (context);
  g_slist_free_full (line.runs, (GDestroyNotify) pango_glyph_item_free);
  g_object_unref (renderer);
  g_object_unref (font);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/skip-ink/intersections", test_intersections);
  g_test_add_func ("/skip-ink/cache", test_cache);
  g_test_add_func ("/skip-ink/renderer", test_renderer);
  g_test_add_func ("/skip-ink/merged-runs", test_merged_runs);
  g_test_add_func ("/skip-ink/gap-order", test_gap_order);
  g_test_add_func ("/skip-ink/centered-baseline", test_centered_baseline);
  g_test_add_func ("/skip-ink/exclusions", test_exclusions);
  g_test_add_func ("/skip-ink/decorations", test_decorations);
  g_test_add_func ("/skip-ink/color-boundary", test_color_boundary);

  return g_test_run ();
}
