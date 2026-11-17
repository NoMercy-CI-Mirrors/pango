/* pango-ink.c
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

#include <math.h>
#include <hb.h>

#include "pango-ink-private.h"
#include "pango-glyph.h"

#define INK_TOLERANCE (PANGO_SCALE / 64.0)
#define INK_CACHE_SIZE 1024

typedef struct
{
  PangoFont *font;
  PangoGlyph glyph;
  double top, bottom;
  double left, right;
} InkInterval;

typedef struct
{
  double x, y;
} InkPoint;

static guint
ink_interval_hash (gconstpointer data)
{
  const InkInterval *interval = data;

  return g_direct_hash (interval->font) ^ interval->glyph ^
         g_double_hash (&interval->top) ^ (g_double_hash (&interval->bottom) << 1);
}

static gboolean
ink_interval_equal (gconstpointer a,
                    gconstpointer b)
{
  const InkInterval *ia = a;
  const InkInterval *ib = b;

  return ia->font == ib->font && ia->glyph == ib->glyph &&
         ia->top == ib->top && ia->bottom == ib->bottom;
}

static void
ink_interval_free (gpointer data)
{
  InkInterval *interval = data;

  g_object_unref (interval->font);
  g_free (interval);
}

GHashTable *
_pango_ink_cache_new (void)
{
  return g_hash_table_new_full (ink_interval_hash, ink_interval_equal,
                                ink_interval_free, NULL);
}

static void
ink_line (InkInterval *interval,
          InkPoint     a,
          InkPoint     b,
          double       tolerance)
{
  double top = interval->top - tolerance;
  double bottom = interval->bottom + tolerance;
  double start = 0;
  double end = 1;
  double x1, x2;

  if (MAX (a.y, b.y) < top || MIN (a.y, b.y) > bottom)
    return;

  if (a.y != b.y)
    {
      double t1 = (top - a.y) / (b.y - a.y);
      double t2 = (bottom - a.y) / (b.y - a.y);

      start = MAX (0, MIN (t1, t2));
      end = MIN (1, MAX (t1, t2));
    }

  x1 = a.x + start * (b.x - a.x);
  x2 = a.x + end * (b.x - a.x);
  interval->left = MIN (interval->left, MIN (x1, x2) - tolerance);
  interval->right = MAX (interval->right, MAX (x1, x2) + tolerance);
}

static InkPoint
ink_midpoint (InkPoint a,
              InkPoint b)
{
  return (InkPoint) { (a.x + b.x) * 0.5, (a.y + b.y) * 0.5 };
}

static void
ink_curve (InkInterval *interval,
           InkPoint     a,
           InkPoint     b,
           InkPoint     c,
           InkPoint     d,
           unsigned int depth)
{
  double top = MIN (MIN (a.y, b.y), MIN (c.y, d.y));
  double bottom = MAX (MAX (a.y, b.y), MAX (c.y, d.y));
  double error;
  InkPoint ab, bc, cd, abc, bcd, middle;

  if (bottom < interval->top || top > interval->bottom)
    return;

  error = MAX (MAX (fabs (b.x - (2 * a.x + d.x) / 3),
                    fabs (b.y - (2 * a.y + d.y) / 3)),
               MAX (fabs (c.x - (a.x + 2 * d.x) / 3),
                    fabs (c.y - (a.y + 2 * d.y) / 3)));

  if (error <= INK_TOLERANCE)
    {
      ink_line (interval, a, d, error);
      return;
    }

  if (depth == 16)
    {
      interval->left = MIN (interval->left, MIN (MIN (a.x, b.x), MIN (c.x, d.x)));
      interval->right = MAX (interval->right, MAX (MAX (a.x, b.x), MAX (c.x, d.x)));
      return;
    }

  ab = ink_midpoint (a, b);
  bc = ink_midpoint (b, c);
  cd = ink_midpoint (c, d);
  abc = ink_midpoint (ab, bc);
  bcd = ink_midpoint (bc, cd);
  middle = ink_midpoint (abc, bcd);

  ink_curve (interval, a, ab, abc, middle, depth + 1);
  ink_curve (interval, middle, bcd, cd, d, depth + 1);
}

static void
ink_line_to (hb_draw_funcs_t *funcs,
             void            *data,
             hb_draw_state_t *state,
             float            x,
             float            y,
             void            *user_data)
{
  ink_line (data,
            (InkPoint) { state->current_x, -state->current_y },
            (InkPoint) { x, -y }, 0);
}

static void
ink_cubic_to (hb_draw_funcs_t *funcs,
              void            *data,
              hb_draw_state_t *state,
              float            x1,
              float            y1,
              float            x2,
              float            y2,
              float            x3,
              float            y3,
              void            *user_data)
{
  ink_curve (data,
             (InkPoint) { state->current_x, -state->current_y },
             (InkPoint) { x1, -y1 },
             (InkPoint) { x2, -y2 },
             (InkPoint) { x3, -y3 }, 0);
}

static void
ink_close_path (hb_draw_funcs_t *funcs,
                void            *data,
                hb_draw_state_t *state,
                void            *user_data)
{
  ink_line (data,
            (InkPoint) { state->current_x, -state->current_y },
            (InkPoint) { state->path_start_x, -state->path_start_y }, 0);
}

static hb_draw_funcs_t *
ink_draw_funcs (void)
{
  static gsize funcs;

  if (g_once_init_enter (&funcs))
    {
      hb_draw_funcs_t *draw_funcs = hb_draw_funcs_create ();

      hb_draw_funcs_set_line_to_func (draw_funcs, ink_line_to, NULL, NULL);
      hb_draw_funcs_set_cubic_to_func (draw_funcs, ink_cubic_to, NULL, NULL);
      hb_draw_funcs_set_close_path_func (draw_funcs, ink_close_path, NULL, NULL);
      hb_draw_funcs_make_immutable (draw_funcs);
      g_once_init_leave (&funcs, (gsize) draw_funcs);
    }

  return (hb_draw_funcs_t *) funcs;
}

gboolean
_pango_ink_get_interval (GHashTable *cache,
                         PangoFont  *font,
                         PangoGlyph  glyph,
                         double      top,
                         double      bottom,
                         double     *left,
                         double     *right)
{
  InkInterval key = { font, glyph, top, bottom, G_MAXDOUBLE, -G_MAXDOUBLE };
  InkInterval *interval;

  g_assert (cache != NULL);
  g_assert (PANGO_IS_FONT (font));
  g_assert (top <= bottom);
  g_assert (left != NULL && right != NULL);

  if (glyph == PANGO_GLYPH_EMPTY || (glyph & PANGO_GLYPH_UNKNOWN_FLAG))
    return FALSE;

  interval = g_hash_table_lookup (cache, &key);
  if (!interval)
    {
      hb_font_t *hb_font = pango_font_get_hb_font (font);
      hb_glyph_extents_t extents;

      if (g_hash_table_size (cache) >= INK_CACHE_SIZE)
        g_hash_table_remove_all (cache);

      interval = g_memdup2 (&key, sizeof key);
      g_object_ref (font);

      if (!hb_font_get_glyph_extents (hb_font, glyph, &extents) ||
          !(- (double) extents.y_bearing > bottom ||
            - (double) extents.y_bearing - extents.height < top))
        hb_font_draw_glyph (hb_font, glyph, ink_draw_funcs (), interval);

      g_hash_table_add (cache, interval);
    }

  *left = interval->left;
  *right = interval->right;

  return interval->left < interval->right;
}
