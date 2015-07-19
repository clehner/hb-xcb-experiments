PKGS = harfbuzz freetype2 xcb xcb-render

CFLAGS = -Wall -Werror -Wno-unused `pkg-config --cflags $(PKGS)` -g
LDFLAGS = `pkg-config --libs $(PKGS)` -lm

FONT = /usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf
TEXT = "This is some text"

demo: hello-harfbuzz
	./$< $(FONT) $(TEXT)

debug: hello-harfbuzz
	gdb --args ./$< /usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf hioiasdfoiadsjf

%: %.c
	$(CC) -std=c99 -o $@ $^ $(CFLAGS) $(LDFLAGS)
