/*
 * deepzoom_tiler.c - A program to create image tiles for a deepzoom viewer
 *
 * Copyright (C) 2011	Andrew Clayton <andrew@digital-domain.net>
 * Released under the GNU General Public License (GPL) version 3.
 * See COPYING
 */

#define _XOPEN_SOURCE 500	/* For NAME_MAX & PATH_MAX */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <getopt.h>

#include <magick/api.h>

#define MAX(a, b)	(((a) > (b)) ? (a) : (b))
#define TILE_SZ		tile_sz
#define OVERLAP		overlap

int tile_sz = 256;
int overlap = 0;

static void print_usage(void)
{
	printf("Usage: deepzoom_tiler [-t tile size] [-o overlap size] [-h] "
								"<image>\n");
}

static void write_dzi(char *dzi, char *type, unsigned int width,
							unsigned int height)
{
	FILE *fp = fopen(dzi, "w");

	/* It seems the contents of this file are case sensitive */
	fprintf(fp, "<?xml version = \"1.0\" encoding = \"UTF-8\" ?>\n");
	fprintf(fp, "<Image Format = \"%s\" Overlap = \"%d\" TileSize = "
			"\"%d\" xmlns = "
			"\"http://schemas.microsoft.com/deepzoom/2008\">\n",
						type, OVERLAP, TILE_SZ);
	fprintf(fp, "\t<Size Width = \"%u\" Height = \"%u\" />\n",
								width, height);
	fprintf(fp, "</Image>\n");

	fclose(fp);
}

static void create_level(Image *image, char *type, int level,
							unsigned long width,
							unsigned long height,
							unsigned int columns,
							unsigned int rows)
{
	int i;
	unsigned int row = 0;
	unsigned int col = 0;
	unsigned int xoff = 0;
	unsigned int yoff = 0;
	char dname[NAME_MAX + 1];
	Image *rimage;
	ImageInfo *image_info;
	ExceptionInfo exception;
	RectangleInfo cropper;

	GetExceptionInfo(&exception);
	image_info = CloneImageInfo((ImageInfo *)NULL);
	rimage = ResizeImage(image, width, height, LanczosFilter, 1.0,
								&exception);
	memset(&cropper, 0, sizeof(cropper));

	snprintf(dname, NAME_MAX + 1, "%d", level);
	mkdir(dname, 0777);

	/* We create the tiles by columns */
	for (i = 0; i < columns * rows; i++) {
		char tname[PATH_MAX];
		Image *tile;

		if (i % rows == 0 && i != 0) {
			/* Start a new column */
			col++;
			row = 0;
			xoff = col * TILE_SZ;
			yoff = 0;
		}

		/*
		 * A lot of the complexity here is to cater for having
		 * overlap in the tiles.
		 */

		/* Are we the top left tile? */
		if (col == 0 && row == 0) {
			cropper.width = TILE_SZ + OVERLAP;
			cropper.height = TILE_SZ + OVERLAP;
		/* Are we the top right tile? */
		} else if (col == columns - 1 && row == 0) {
			cropper.width = TILE_SZ + OVERLAP;
			cropper.height = TILE_SZ + OVERLAP;
			cropper.x = xoff - OVERLAP;
			cropper.y = yoff;
		/* Are we the bottom left tile? */
		} else if (col == 0 && row == rows - 1) {
			cropper.width = TILE_SZ + OVERLAP;
			cropper.height = TILE_SZ + OVERLAP;
			cropper.y = yoff - OVERLAP;
		/* Are we the bottom right tile? */
		} else if (col == columns - 1 && row == rows - 1) {
			cropper.width = TILE_SZ + OVERLAP;
			cropper.height = TILE_SZ + OVERLAP;
			cropper.x = xoff - OVERLAP;
			cropper.y = yoff - OVERLAP;
		/* Are we on the top edge? */
		} else if (row == 0) {
			cropper.width = TILE_SZ + (OVERLAP * 2);
			cropper.height = TILE_SZ + OVERLAP;
			cropper.x = xoff - OVERLAP;
			cropper.y = yoff;
		/* Are we on the bottom edge? */
		} else if (row == rows - 1) {
			cropper.width = TILE_SZ + (OVERLAP * 2);
			cropper.height = TILE_SZ + OVERLAP;
			cropper.x = xoff - OVERLAP;
			cropper.y = yoff - OVERLAP;
		/* Are we on the left hand edge? */
		} else if (col == 0) {
			cropper.width = TILE_SZ + OVERLAP;
			cropper.height = TILE_SZ + (OVERLAP * 2);
			cropper.y = yoff - OVERLAP;
		/* Are we on the right hand edge? */
		} else if (col == columns - 1) {
			cropper.width = TILE_SZ + OVERLAP;
			cropper.height = TILE_SZ + (OVERLAP * 2);
			cropper.x = xoff - OVERLAP;
			cropper.y = yoff - OVERLAP;
		/* We are an internal tile */
		} else {
			cropper.width = TILE_SZ + (OVERLAP * 2);
			cropper.height = TILE_SZ + (OVERLAP * 2);
			cropper.x = xoff - OVERLAP;
			cropper.y = yoff - OVERLAP;
		}

		tile = CropImage(rimage, &cropper, &exception);
		snprintf(tname, PATH_MAX, "%d/%d_%d.%s", level, col, row,
									type);
		strcpy(tile->filename, tname);
		WriteImage(image_info, tile);
		DestroyImage(tile);

		/* Adjust the y (height) offset for the next row */
		yoff += TILE_SZ;
		row++;
	}

	DestroyImage(rimage);
	DestroyImageInfo(image_info);
	DestroyExceptionInfo(&exception);
}

static void create_levels(Image *image, char *type, unsigned long width,
							unsigned long height)
{
	unsigned int max_level;
	unsigned int columns;
	unsigned int rows;
	int level;

	/* How many times can we half the image in size? */
	max_level = ceil(log2(MAX(width, height)));

	for (level = max_level; level >= 0; level--) {
		columns = ceil((float)width / TILE_SZ);
		rows = ceil((float)height / TILE_SZ);

		printf("level %d is %lu x %lu (%u columns, %u rows)\n",
					level, width, height, columns, rows);
		create_level(image, type, level, width, height, columns, rows);

		width = (width + 1) >> 1; /* +1 for the effects of ceil */
		height = (height + 1) >> 1;
	}
}

int main(int argc, char **argv)
{
	ExceptionInfo exception;
	ImageInfo *image_info;
	Image *image;
	char *ext;
	char image_name[NAME_MAX + 1];
	char deepzoom_directory[NAME_MAX + 1];
	char dzi[NAME_MAX];
	int opt;

	while ((opt = getopt(argc, argv, "t:o:h")) != -1) {
		switch (opt) {
		case 't':
			tile_sz = atoi(optarg);
			break;
		case 'o':
			overlap = atoi(optarg);
			break;
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
		}
	}

	if (optind < argc) {
		snprintf(image_name, NAME_MAX + 1, "%s", argv[optind]);
	} else {
		print_usage();
		exit(EXIT_FAILURE);
	}

	InitializeMagick(NULL);
	GetExceptionInfo(&exception);
	image_info = CloneImageInfo((ImageInfo *)NULL);
	strcpy(image_info->filename, image_name);
	image = ReadImage(image_info, &exception);
	if (exception.severity != UndefinedException)
		CatchImageException(image);

	ext = rindex(image_name, '.');
	strncpy(deepzoom_directory, image_name, strlen(image_name) -
								strlen(ext));
	deepzoom_directory[strlen(image_name) - strlen(ext)] = '\0';
	strcpy(dzi, deepzoom_directory);
	strncat(deepzoom_directory, "_files", NAME_MAX -
						strlen(deepzoom_directory));
	printf("Creating directory: %s\n", deepzoom_directory);
	mkdir(deepzoom_directory, 0777);
	chdir(deepzoom_directory);
	create_levels(image, ext + 1, image->columns, image->rows);
	chdir("..");

	strncat(dzi, ".dzi", NAME_MAX - strlen(dzi));
	printf("Writing DZI: %s\n", dzi);
	write_dzi(dzi, ext + 1, image->columns, image->rows);

	DestroyImage(image);
	DestroyImageInfo(image_info);
	DestroyExceptionInfo(&exception);
	DestroyMagick();

	exit(EXIT_SUCCESS);
}
