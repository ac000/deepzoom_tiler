CC=gcc
CFLAGS=-Wall -std=c99 -O2
INCS=`GraphicsMagick-config --cppflags`
LIBS=`GraphicsMagick-config --libs`
LD=`GraphicsMagick-config --ldflags` -lm

deepzoom_tiler: deepzoom_tiler.c
	$(CC) $(CFLAGS) -o deepzoom_tiler deepzoom_tiler.c $(INCS) $(LIBS) $(LD)

clean:
	rm deepzoom_tiler
