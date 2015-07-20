# text rendering experiments

This is an attempt to do text rendering using XCB, FreeType, and HarfBuzz.

It's not completely working and the code is very messy.
However, perhaps it may be useful as a starting point.

## References
- [The X Rendering Extension](http://www.x.org/releases/X11R7.6/doc/renderproto/renderproto.txt)
- [cairo](http://cgit.freedesktop.org/cairo/tree/src/cairo-xcb-connection-render.c)
  the only code I could find using xcb\_render/glyphsets
- [libXft](http://cgit.freedesktop.org/xorg/lib/libXft/tree/src/xftglyphs.c)
  the old way
- [FreeType Tutorial](http://www.freetype.org/freetype2/docs/tutorial/step1.html)
- [harfbuzz tutorial](https://github.com/behdad/harfbuzz-tutorial)
  from which this is forked. shows how to use HarfBuzz and FreeType with Cairo
- [node-x11/render-glyph.js](https://github.com/sidorares/node-x11/blob/master/examples/simple/text/render-glyph.js)
  shows how to use XRender with node-x11
