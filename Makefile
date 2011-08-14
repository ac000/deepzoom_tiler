deepzoom_tiler: deepzoom_tiler.c
	gcc -g -Wall -o deepzoom_tiler deepzoom_tiler.c -lm `GraphicsMagick-config --cppflags --ldflags --libs`
