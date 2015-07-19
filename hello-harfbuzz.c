#include <alloca.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <hb.h>
#include <hb-ft.h>
#include <xcb/xcb.h>
#include <xcb/render.h>

#define FONT_SIZE 10
#define MARGIN (FONT_SIZE * .5)


xcb_render_pictformat_t get_pictformat_from_visual(xcb_render_query_pict_formats_reply_t *reply, xcb_visualid_t visual);
xcb_render_pictforminfo_t *get_pictforminfo(xcb_render_query_pict_formats_reply_t *reply, xcb_render_pictforminfo_t *query);


static void
put_str(xcb_connection_t *c, xcb_drawable_t drawable, xcb_gcontext_t gc,
    char *str)
{
  xcb_generic_event_t *e;
  int done = 0;
  xcb_image_text_8 (c, strlen(str), drawable, gc, 20, 20, str);
  xcb_flush(c);
  while (!done && (e = xcb_wait_for_event (c))) {
    xcb_generic_error_t *err = (xcb_generic_error_t *)e;
    switch (e->response_type & ~0x80) {
    case XCB_KEY_PRESS:
      done = 1;
    case 0:
      printf("Received X11 error %d\n", err->error_code);
    }
    free (e);
  }
}

static void
testCookie (xcb_void_cookie_t cookie,
			xcb_connection_t *connection,
			char *errMessage )
{
	xcb_generic_error_t *error = xcb_request_check (connection, cookie);
	if (error) {
		printf ("ERROR: %s : %"PRIu8"\n", errMessage , error->error_code);
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
  if (0) {
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
  xcb_void_cookie_t cookie;
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

  /* map the window on the screen */
  xcb_map_window_checked (c, win);
  testCookie(cookie, c, "can't map window");
  xcb_flush (c);

  xcb_rectangle_t window_rect = {
    .x = 10,
    .y = 10,
    .width = 20,
    .height = 20
  };
  xcb_render_color_t back_color = {
    .red = 0xffff,
    .green = 0xcccc,
    .blue = 0x9999,
    .alpha = 0xffff
  };

  xcb_rectangle_t rectangles[] = {
    {40, 40, 20, 20},
  };

  xcb_flush(c);
  while ((e = xcb_wait_for_event (c))) {
    switch (e->response_type & ~0x80) {
    case XCB_EXPOSE:
      goto endloop1;
      /*
      xcb_poly_rectangle_checked (c, win, foreground, 1, rectangles);
      testCookie(cookie, c, "can't poly rectangle");
      xcb_flush (c);
      */
      break;
    case XCB_KEY_PRESS:
      goto endloop1;
    }
    free (e);
  }
  endloop1:{}

      /*
  cairo_t *cr;
  cr = cairo_create (cairo_surface);
  cairo_set_source_rgba (cr, 1., 1., 1., 1.);
  cairo_paint (cr);
  cairo_set_source_rgba (cr, 0., 0., 0., 1.);
  cairo_translate (cr, MARGIN, MARGIN);
  */

  xcb_render_query_version_cookie_t version_cookie;
  xcb_render_query_version_reply_t *version;
  version_cookie = xcb_render_query_version (c,
      XCB_RENDER_MAJOR_VERSION, XCB_RENDER_MINOR_VERSION);
  // testCookie(version_cookie, c, "can't query version");
  version = xcb_render_query_version_reply (c, version_cookie, 0);
  if (!version) {
    printf("no render version\n");
    return 1;
  }
  printf("render version: %u.%u\n",
      version->major_version, version->minor_version);

  xcb_render_glyphset_t gsid;
  xcb_render_pictformat_t format, window_format;
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
    if (formats[i].type == XCB_RENDER_PICT_TYPE_DIRECT &&
	formats[i].depth == 32) {
      format = xcb_render_query_pict_formats_formats(formats_reply)->id;
      break;
    }
  }
  /* does the reply need to be freed? */

  /* Get the xcb_render_pictformat_t associated with the window. */
  window_format = get_pictformat_from_visual(formats_reply, screen->root_visual);

  /* Setting query so that it will search for an 8 bit alpha surface. */
  xcb_render_pictforminfo_t    *alpha_forminfo_ptr, query;
  xcb_render_pictformat_t      alpha_mask_format;

  query.id = 0;
  query.type = XCB_RENDER_PICT_TYPE_DIRECT;
  query.depth = 8;
  query.direct.red_mask = 0;
  query.direct.green_mask = 0;
  query.direct.blue_mask = 0;
  query.direct.alpha_mask = 255;

  /* Get the xcb_render_pictformat_t we will use for the alpha mask */
  alpha_forminfo_ptr = get_pictforminfo(formats_reply, &query);

  alpha_mask_format = alpha_forminfo_ptr->id;


  /* create picture to composite into */
  xcb_render_picture_t window_pict = xcb_generate_id(c);
  xcb_render_create_picture_checked(c, window_pict, win, window_format, 0, 0);
  testCookie(cookie, c, "can't create picture");

  cookie = xcb_render_fill_rectangles_checked(c, XCB_RENDER_PICT_OP_OVER,
      window_pict, back_color, 1, &window_rect);
  testCookie(cookie, c, "can't fill rectangles");

  /*
  iter = xcb_setup_roots_iterator (xcb_get_setup (c));
  for (; iter.rem; --screen, xcb_render_pictformat_next (&iter)) {
    format = iter.data;
    printf("got pictformat\n");
    break;
  }
  */


  gsid = xcb_generate_id (c);
  cookie = xcb_render_create_glyph_set_checked (c, gsid, alpha_mask_format);
  testCookie(cookie, c, "can't create glyph set");

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
    /* convert character code to glyph index? */
    // glyph_index = FT_Get_Char_Index( face, info[i].codepoint );

    hb_codepoint_t gid = info[i].codepoint;
    char glyphname[32];
    hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));

    // printf ("%u. glyph='%s'\n", i, glyphname);

    /* load the glyph */
    ft_error = FT_Load_Glyph (ft_face, info[i].codepoint, FT_LOAD_RENDER);
    if (ft_error) {
      printf("error loading glyph\n");
      continue;
    }

    FT_GlyphSlot slot = ft_face->glyph;
    FT_Bitmap *bitmap = &slot->bitmap;

    printf("bitmap size=(%u,%u)\n",
	bitmap->width, bitmap->rows);

    /* copy into glyph data for xcb */
    glyph.width = 4; // bitmap->width;
    glyph.height = 4; // bitmap->rows;
    glyph.x = 0;
    glyph.y = 0; /* negative? 0? */
    glyph.x_off = 0; // pos[i].x_advance / 64.;
    glyph.y_off = 0; // pos[i].y_advance / 64.;
    glyph_id = info[i].codepoint;

    buf_size = 16;
    buf = alloca(buf_size);
    /*
    if (buf_size & 3)
      buf_size += 4 - (buf_size & 3);
    if ((size_t)buf & 3)
      buf += 4 - ((size_t)buf & 3);
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

    cookie = xcb_render_add_glyphs_checked (c, gsid, 1,
	&glyph_id, &glyph, buf_size, buf);
    testCookie(cookie, c, "can't add glyph");
/*
    xcb_render_add_glyphs (c, gsid, 1, &glyph_id, &glyph, buf_size, buf);
*/
  }


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


  struct glyph_header {
    uint8_t count;
    uint8_t pad0[3];
    int16_t dx, dy;
  } *glyph_header;

  uint8_t *glyphitems_buf;
  uint32_t glyphitems_len = 0;

  glyphitems_buf = alloca(len * (1 + sizeof *glyph_header));

    /*
  cairo_glyph_t *cairo_glyphs = cairo_glyph_allocate (len);
  */
  double current_x = 0;
  double current_y = 0;
  for (unsigned int i = 0; i < len; i++)
  {
    struct glyph_header glyph_header = {
      .count = 1,
      .dx = current_x + pos[i].x_offset / 64.,
      .dy = -(current_y + pos[i].y_offset / 64.),
    };
    memcpy(glyphitems_buf + glyphitems_len, &glyph_header, sizeof glyph_header);
    glyphitems_len += sizeof(struct glyph_header);
    glyphitems_buf[glyphitems_len] = info[i].codepoint;
    glyphitems_len += 4;

    current_x += pos[i].x_advance / 64.;
    current_y += pos[i].y_advance / 64.;

    /*
    if (glyphitems_len & 3)
      glyphitems_len += 4 - (glyphitems_len & 3);
      */
  }

  int16_t src_x = 0;
  int16_t src_y = 0;

  /* create src picture */
  xcb_render_picture_t src = xcb_generate_id(c);
  xcb_render_create_picture_checked(c, src, win, window_format, 0, 0);
  testCookie(cookie, c, "can't create source picture");

  cookie = xcb_render_composite_glyphs_8_checked (c,
      XCB_RENDER_PICT_OP_ADD, src, window_pict, alpha_mask_format, gsid,
    src_x, src_y, glyphitems_len, glyphitems_buf);
  testCookie(cookie, c, "can't composite glyphs");
  /*
	Errors:
		Picture, PictOp, PictFormat, GlyphSet, Glyph
		*/

  xcb_render_free_picture(c, window_pict);

  /*
  cairo_surface_write_to_png (cairo_surface, "out.png");
  */

  while ((e = xcb_wait_for_event (c))) {
  xcb_generic_error_t *err = (xcb_generic_error_t *)e;
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
    case 0:
      printf("Received X11 error %d\n", err->error_code);
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
  xcb_disconnect (c);

  hb_buffer_destroy (hb_buffer);
  hb_font_destroy (hb_font);

  FT_Done_Face (ft_face);
  FT_Done_FreeType (ft_library);

  return 0;
}


/**********************************************************
 * This function searches through the reply for a 
 * PictVisual who's xcb_visualid_t is the same as the one
 * specified in query. The function will then return the
 * xcb_render_pictformat_t from that PictVIsual structure. 
 * This is useful for getting the xcb_render_pictformat_t that is
 * the same visual type as the root window.
 **********************************************************/
/* from http://cgit.freedesktop.org/xcb/demo/tree/rendertest.c */
xcb_render_pictformat_t get_pictformat_from_visual(xcb_render_query_pict_formats_reply_t *reply, xcb_visualid_t query)
{
    xcb_render_pictscreen_iterator_t screen_iter;
    xcb_render_pictscreen_t    *cscreen;
    xcb_render_pictdepth_iterator_t  depth_iter;
    xcb_render_pictdepth_t     *cdepth;
    xcb_render_pictvisual_iterator_t visual_iter; 
    xcb_render_pictvisual_t    *cvisual;
    xcb_render_pictformat_t  return_value;
    
    screen_iter = xcb_render_query_pict_formats_screens_iterator(reply);

    while(screen_iter.rem)
    {
        cscreen = screen_iter.data;
        
        depth_iter = xcb_render_pictscreen_depths_iterator(cscreen);
        while(depth_iter.rem)
        {
            cdepth = depth_iter.data;

            visual_iter = xcb_render_pictdepth_visuals_iterator(cdepth);
            while(visual_iter.rem)
            {
                cvisual = visual_iter.data;

                if(cvisual->visual == query)
                {
                    return cvisual->format;
                }
                xcb_render_pictvisual_next(&visual_iter);
            }
            xcb_render_pictdepth_next(&depth_iter);
        }
        xcb_render_pictscreen_next(&screen_iter);
    }
    return_value = 0;
    return return_value;
}


xcb_render_pictforminfo_t *get_pictforminfo(xcb_render_query_pict_formats_reply_t *reply, xcb_render_pictforminfo_t *query)
{
    xcb_render_pictforminfo_iterator_t forminfo_iter;
    
    forminfo_iter = xcb_render_query_pict_formats_formats_iterator(reply);

    while(forminfo_iter.rem)
    {
        xcb_render_pictforminfo_t *cformat;
        cformat  = forminfo_iter.data;
        xcb_render_pictforminfo_next(&forminfo_iter);

        if( (query->id != 0) && (query->id != cformat->id) )
        {
            continue;
        }

        if(query->type != cformat->type)
        {
            continue;
        }
        
        if( (query->depth != 0) && (query->depth != cformat->depth) )
        {
            continue;
        }
        
        if( (query->direct.red_mask  != 0)&& (query->direct.red_mask != cformat->direct.red_mask))
        {
            continue;
        }
        
        if( (query->direct.green_mask != 0) && (query->direct.green_mask != cformat->direct.green_mask))
        {
            continue;
        }
        
        if( (query->direct.blue_mask != 0) && (query->direct.blue_mask != cformat->direct.blue_mask))
        {
            continue;
        }
        
        if( (query->direct.alpha_mask != 0) && (query->direct.alpha_mask != cformat->direct.alpha_mask))
        {
            continue;
        }
        
        /* This point will only be reached if the pict format   *
         * matches what the user specified                      */
        return cformat; 
    }
    
    return NULL;
}

