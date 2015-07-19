#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <hb.h>
#include <hb-ft.h>
#include <xcb/xcb.h>
#include <xcb/render.h>

#define FONT_SIZE 36
#define MARGIN (FONT_SIZE * .5)

static void
testCookie (xcb_void_cookie_t cookie,
			xcb_connection_t *connection,
			char *errMessage )
{
	xcb_generic_error_t *error = xcb_request_check (connection, cookie);
	if (error) {
		fprintf (stderr, "ERROR: %s : %"PRIu8"\n", errMessage , error->error_code);
		xcb_disconnect (connection);
		exit (-1);
	}
}

int
main(int argc, char **argv)
{
  const char *fontfile;
  const char *text;

  if (argc < 3)
  {
    fprintf (stderr, "usage: hello-harfbuzz font-file.ttf text\n");
    exit (1);
  }

  fontfile = argv[1];
  text = argv[2];

  /* Initialize FreeType and create FreeType font face. */
  FT_Library ft_library;
  FT_Face ft_face;
  FT_Error ft_error;

  if ((ft_error = FT_Init_FreeType (&ft_library)))
    abort();
  if ((ft_error = FT_New_Face (ft_library, fontfile, 0, &ft_face)))
    abort();
  if ((ft_error = FT_Set_Char_Size (ft_face, FONT_SIZE*64, FONT_SIZE*64, 0, 0)))
    abort();

  /* Create hb-ft font. */
  hb_font_t *hb_font;
  hb_font = hb_ft_font_create (ft_face, NULL);

  /* Create hb-buffer and populate. */
  hb_buffer_t *hb_buffer;
  hb_buffer = hb_buffer_create ();
  hb_buffer_add_utf8 (hb_buffer, text, -1, 0, -1);
  hb_buffer_guess_segment_properties (hb_buffer);

  /* Shape it! */
  hb_shape (hb_font, hb_buffer, NULL, 0);

  /* Get glyph information and positions out of the buffer. */
  unsigned int len = hb_buffer_get_length (hb_buffer);
  hb_glyph_info_t *info = hb_buffer_get_glyph_infos (hb_buffer, NULL);
  hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (hb_buffer, NULL);

  /* Print them out as is. */
  printf ("Raw buffer contents:\n");
  for (unsigned int i = 0; i < len; i++)
  {
    hb_codepoint_t gid   = info[i].codepoint;
    unsigned int cluster = info[i].cluster;
    double x_advance = pos[i].x_advance / 64.;
    double y_advance = pos[i].y_advance / 64.;
    double x_offset  = pos[i].x_offset / 64.;
    double y_offset  = pos[i].y_offset / 64.;

    char glyphname[32];
    hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));

    printf ("glyph='%s'	cluster=%d	advance=(%g,%g)	offset=(%g,%g)\n",
            glyphname, cluster, x_advance, y_advance, x_offset, y_offset);
  }

  printf ("Converted to absolute positions:\n");
  /* And converted to absolute positions. */
  {
    double current_x = 0;
    double current_y = 0;
    for (unsigned int i = 0; i < len; i++)
    {
      hb_codepoint_t gid   = info[i].codepoint;
      unsigned int cluster = info[i].cluster;
      double x_position = current_x + pos[i].x_offset / 64.;
      double y_position = current_y + pos[i].y_offset / 64.;


      char glyphname[32];
      hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));

      printf ("glyph='%s'	cluster=%d	position=(%g,%g)\n",
	      glyphname, cluster, x_position, y_position);

      current_x += pos[i].x_advance / 64.;
      current_y += pos[i].y_advance / 64.;
    }
  }

  /* Draw, using xcb. */
  double width = 2 * MARGIN;
  double height = 2 * MARGIN;
  for (unsigned int i = 0; i < len; i++)
  {
    width  += pos[i].x_advance / 64.;
    height -= pos[i].y_advance / 64.;
  }
  if (HB_DIRECTION_IS_HORIZONTAL (hb_buffer_get_direction(hb_buffer)))
    height += FONT_SIZE;
  else
    width  += FONT_SIZE;


  xcb_connection_t    *c;
  xcb_screen_t        *screen;
  xcb_drawable_t       win;
  xcb_gcontext_t       foreground;
  xcb_gcontext_t       background;
  xcb_generic_event_t *e;
  uint32_t             mask = 0;
  uint32_t             values[2];

  c = xcb_connect (NULL, NULL);

  /* get the first screen */
  screen = xcb_setup_roots_iterator (xcb_get_setup (c)).data;

  /* root window */
  win = screen->root;

  /* create black (foreground) graphic context */
  foreground = xcb_generate_id (c);
  mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
  values[0] = screen->black_pixel;
  values[1] = 0;
  xcb_create_gc (c, foreground, win, mask, values);

  /* create white (background) graphic context */
  background = xcb_generate_id (c);
  mask = XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
  values[0] = screen->white_pixel;
  values[1] = 0;
  xcb_create_gc (c, background, win, mask, values);

  /* create the window */
  win = xcb_generate_id(c);
  mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  values[0] = screen->white_pixel;
  values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS;
  xcb_create_window (c,                             /* connection    */
                     XCB_COPY_FROM_PARENT,          /* depth         */
                     win,                           /* window Id     */
                     screen->root,                  /* parent window */
                     0, 0,                          /* x, y          */
                     150, 150,                      /* width, height */
                     10,                            /* border_width  */
                     XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class         */
                     screen->root_visual,           /* visual        */
                     mask, values);                 /* masks         */

  /* Set up cairo surface. */
      /*
  cairo_surface_t *cairo_surface;
  cairo_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					      ceil(width),
					      ceil(height));
					      */
      /*
  cairo_t *cr;
  cr = cairo_create (cairo_surface);
  cairo_set_source_rgba (cr, 1., 1., 1., 1.);
  cairo_paint (cr);
  cairo_set_source_rgba (cr, 0., 0., 0., 1.);
  cairo_translate (cr, MARGIN, MARGIN);
  */

  xcb_render_glyphset_t gsid;
  xcb_render_pictformat_t format;
  xcb_render_pictforminfo_t *formats;

  /* get a pict format */

  xcb_render_query_pict_formats_cookie_t formats_cookie =
    xcb_render_query_pict_formats (c);
  xcb_render_query_pict_formats_reply_t *formats_reply =
    xcb_render_query_pict_formats_reply (c, formats_cookie, NULL);
  if (!formats_reply) {
    printf("query pict formats failed\n");
    exit(1);
  }
  formats = xcb_render_query_pict_formats_formats(formats_reply);

  for (unsigned int i = 0; i < formats_reply->num_formats; i++) {
    printf("pict format: type=%u, depth=%u\n", formats[i].type,
	formats[i].depth);
      /* TODO: figure out which one should really be picked */
    if (formats[i].depth == 8) {
      format = xcb_render_query_pict_formats_formats(formats_reply)->id;
      break;
    }
  }
  /* does the reply need to be freed? */

  /*
  iter = xcb_setup_roots_iterator (xcb_get_setup (c));
  for (; iter.rem; --screen, xcb_render_pictformat_next (&iter)) {
    format = iter.data;
    printf("got pictformat\n");
    break;
  }
  */

  gsid = xcb_generate_id (c);
  xcb_void_cookie_t textCookie =
    xcb_render_create_glyph_set_checked (c, gsid, format);
  testCookie(textCookie, c, "can't create glyph set");

  /* Set up cairo font face. */
      /*
  cairo_font_face_t *cairo_face;
  cairo_face = cairo_ft_font_face_create_for_ft_face (ft_face, 0);
  cairo_set_font_face (cr, cairo_face);
  cairo_set_font_size (cr, FONT_SIZE);
  */

  xcb_render_glyphinfo_t glyph;
  uint32_t glyph_id;
  uint8_t *buf;
  uint32_t buf_size;

  for (unsigned int i = 0; i < len; i++)
  {
    /* convert character code to glyph index */
    // glyph_index = FT_Get_Char_Index( face, info[i].codepoint );
    FT_GlyphSlot slot = ft_face->glyph;
    FT_Bitmap *bitmap = &ft_face->glyph->bitmap;

    /* rasterize the glyph */
    FT_Load_Glyph (ft_face, info[i].codepoint, FT_LOAD_RENDER);

    /* copy into glyph data for xcb */
    glyph.width = bitmap->width;
    glyph.height = bitmap->rows;
    glyph.x = slot->bitmap_left;
    glyph.y = slot->bitmap_top; /* negative? 0? */
    glyph.x_off = pos[i].x_advance;
    glyph.y_off = pos[i].y_advance;
    glyph_id = i;

    hb_codepoint_t gid = info[i].codepoint;
    char glyphname[32];
    hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));

    printf ("glyph='%s'	size=(%u,%u)\n",
            glyphname, bitmap->width, bitmap->rows);

    buf_size = bitmap->width * bitmap->rows;
    /*
    buf = alloca(buf_size);
    for (unsigned int x = 0; x < bitmap->width; x++) {
      for (unsigned int y = 0; y < bitmap->rows; y++) {
	buf[x] = bitmap->buffer[x + y * bitmap->width];
      }
    }
    */
    /*
     * more bitmap properties:
    int             pitch;
    unsigned char*  buffer;
    unsigned short  num_grays; // should be 256
    unsigned char   pixel_mode; // should be FT_PIXEL_MODE_GRAY
    unsigned char   palette_mode;
    void*           palette;
    */

    textCookie = xcb_render_add_glyphs_checked (c, gsid, 1,
	&glyph_id, &glyph, buf_size, bitmap->buffer);
    testCookie(textCookie, c, "can't add glyph");

  }

  /* map the window on the screen */
  xcb_map_window (c, win);

  xcb_flush (c);

  /* Set up baseline. */
    /*
  if (HB_DIRECTION_IS_HORIZONTAL (hb_buffer_get_direction(hb_buffer)))
  {
    cairo_font_extents_t font_extents;
    cairo_font_extents (cr, &font_extents);
    double baseline = (FONT_SIZE - font_extents.height) * .5 + font_extents.ascent;
    cairo_translate (cr, 0, baseline);
  }
  else
  {
    cairo_translate (cr, FONT_SIZE * .5, 0);
  }
  */

    /*
  cairo_glyph_t *cairo_glyphs = cairo_glyph_allocate (len);
  */
#if 0
  double current_x = 0;
  double current_y = 0;
  for (unsigned int i = 0; i < len; i++)
  {
    /*
    cairo_glyphs[i].index = info[i].codepoint;
    cairo_glyphs[i].x = current_x + pos[i].x_offset / 64.;
    cairo_glyphs[i].y = -(current_y + pos[i].y_offset / 64.);
    */
    current_x += pos[i].x_advance / 64.;
    current_y += pos[i].y_advance / 64.;
  }
#endif
  /*
  cairo_show_glyphs (cr, cairo_glyphs, len);
  cairo_glyph_free (cairo_glyphs);
  */

  /*
  cairo_surface_write_to_png (cairo_surface, "out.png");
  */
  /*
  xcb_render_composite_glyphs_8(c, op, src, dst, format, gsid,
      src_x, src_y, len,
      xcb_glyphs);
      */

  while ((e = xcb_wait_for_event (c))) {
    switch (e->response_type & ~0x80) {
    case XCB_EXPOSE:
      /*
      xcb_poly_rectangle (c, win, foreground, 1, rectangles);
      xcb_image_text_8 (c, string_len, win, background, 20, 20, string);
      */
      xcb_flush (c);
      break;
    case XCB_KEY_PRESS:
      goto endloop;
    }
    free (e);
  }
  endloop:

  /*
  cairo_font_face_destroy (cairo_face);
  cairo_destroy (cr);
  cairo_surface_destroy (cairo_surface);
  */
  xcb_free_gc (c, foreground);
  xcb_free_gc (c, background);
  xcb_render_free_glyph_set (c, gsid);

  hb_buffer_destroy (hb_buffer);
  hb_font_destroy (hb_font);

  FT_Done_Face (ft_face);
  FT_Done_FreeType (ft_library);

  return 0;
}
