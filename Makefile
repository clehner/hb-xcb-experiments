PKGS = harfbuzz freetype2 xcb xcb-render

CFLAGS = -Wall -Werror -Wno-unused `pkg-config --cflags $(PKGS)` -g
LDFLAGS = `pkg-config --libs $(PKGS)` -lm

FONT = /usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf
TEXT = "This is some text"

demo: hello-harfbuzz-xcb
	./$< $(FONT) $(TEXT)

gdb: hello-harfbuzz-xcb
	gdb --args ./$< $(FONT) $(TEXT)

%: %.c
	$(CC) -std=c99 -o $@ $^ $(CFLAGS) $(LDFLAGS)
