
// Used under LGPLv2.1+
// From https://cgit.freedesktop.org/telepathy/telepathy-gabble/tree/src/util.c
/*
 * util.c - Source for Gabble utility functions
 * Copyright (C) 2006-2007 Collabora Ltd.
 * Copyright (C) 2006-2007 Nokia Corporation
 *   @author Robert McQueen <robert.mcqueen@collabora.co.uk>
 *   @author Simon McVittie <simon.mcvittie@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/** gabble_generate_id:
 *
 * RFC4122 version 4 compliant random UUIDs generator.
 *
 * Returns: A string with RFC41122 version 4 random UUID, must be freed with
 *          g_free().
 */
static gchar *
gabble_generate_id (void)
{
  GRand *grand;
  gchar *str;
  struct {
      guint32 time_low;
      guint16 time_mid;
      guint16 time_hi_and_version;
      guint8 clock_seq_hi_and_rsv;
      guint8 clock_seq_low;
      guint16 node_hi;
      guint32 node_low;
  } uuid;

  /* Fill with random. Every new GRand are seede with 128 bit read from
   * /dev/urandom (or the current time on non-unix systems). This makes the
   * random source good enough for our usage, but may not be suitable for all
   * situation outside Gabble. */
  grand = g_rand_new ();
  uuid.time_low = g_rand_int (grand);
  uuid.time_mid = (guint16) g_rand_int_range (grand, 0, G_MAXUINT16);
  uuid.time_hi_and_version = (guint16) g_rand_int_range (grand, 0, G_MAXUINT16);
  uuid.clock_seq_hi_and_rsv = (guint8) g_rand_int_range (grand, 0, G_MAXUINT8);
  uuid.clock_seq_low = (guint8) g_rand_int_range (grand, 0, G_MAXUINT8);
  uuid.node_hi = (guint16) g_rand_int_range (grand, 0, G_MAXUINT16);
  uuid.node_low = g_rand_int (grand);
  g_rand_free (grand);

  /* Set the two most significant bits (bits 6 and 7) of the
   * clock_seq_hi_and_rsv to zero and one, respectively. */
  uuid.clock_seq_hi_and_rsv = (uuid.clock_seq_hi_and_rsv & 0x3F) | 0x80;

  /* Set the four most significant bits (bits 12 through 15) of the
   * time_hi_and_version field to 4 */
  uuid.time_hi_and_version = (uuid.time_hi_and_version & 0x0fff) | 0x4000;

  str = g_strdup_printf ("%08x-%04x-%04x-%02x%02x-%04x%08x",
    uuid.time_low,
    uuid.time_mid,
    uuid.time_hi_and_version,
    uuid.clock_seq_hi_and_rsv,
    uuid.clock_seq_low,
    uuid.node_hi,
    uuid.node_low);

  return str;
}
