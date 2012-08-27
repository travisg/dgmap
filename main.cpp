#include <cstdio>
#include <cstdlib>
#include <limits.h>

#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <cairo/cairo.h>
#include <png.h>

#include "gamedata.h"

namespace po = boost::program_options;

game_player_vector players;
game_planet_vector planets;

cairo_surface_t *s;
cairo_t *c;
void *canvas;

/* args */
int xres;
int yres;
int zoom;

static void color_to_float(float *color, unsigned int c)
{
//	printf("c 0x%x\n", c);

	color[0] = ((c >> 16) & 0xff) / 255.0f;
	color[1] = ((c >> 8) & 0xff) / 255.0f;
	color[2] = ((c >> 0) & 0xff) / 255.0f;
//	printf("c %f %f %f\n", color[0], color[1], color[2]);
}

static void color_to_float(float *color, const char *str)
{
	color[0] = color[1] = color[2] = 0.0f;	

	// format is #AABBCC

	if (strlen(str) < 7)
		return;

	unsigned int c;
	c = strtoul(&str[1], NULL, 16);

	color_to_float(color, c);
}

/* write a 32bit png file */
static int write_png_file(const char *file_name, uint width, uint height, void *dat)
{
	int y;
	png_byte color_type = PNG_COLOR_TYPE_RGBA;
	png_byte bit_depth = 8; // XXX

	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep * row_pointers;

	uint32_t *data = (uint32_t *)dat;

	row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
	for (y=0; y<height; y++) {
		row_pointers[y] = (png_bytep)&data[y * width];
	}

	/* create file */
	FILE *fp = fopen(file_name, "wb");
	if (!fp) {
		printf("[write_png_file] File %s could not be opened for writing", file_name);
		return -1;
	}

	/* initialize stuff */
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr) {
		printf("[write_png_file] png_create_write_struct failed");
		return -1;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		printf("[write_png_file] png_create_info_struct failed");
		return -1;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		printf("[write_png_file] Error during init_io");
		return -1;
	}

	png_init_io(png_ptr, fp);

	/* write header */
	if (setjmp(png_jmpbuf(png_ptr))) {
		printf("[write_png_file] Error during writing header");
		return -1;
	}

	png_set_IHDR(png_ptr, info_ptr, width, height,
				 bit_depth, color_type, PNG_INTERLACE_NONE,
				 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	/* write bytes */
	if (setjmp(png_jmpbuf(png_ptr))) {
		printf("[write_png_file] Error during writing bytes");
		return -1;
	}

	png_write_image(png_ptr, row_pointers);


	/* end write */
	if (setjmp(png_jmpbuf(png_ptr))) {
		printf("[write_png_file] Error during end of write");
		return -1;
	}

	png_write_end(png_ptr, NULL);

	/* cleanup heap allocation */
	free(row_pointers);

	fclose(fp);

	return 0;
}

static int init_canvas()
{
	size_t len = sizeof(uint32_t) * xres * yres;
	canvas = (uint32_t*)malloc(len);

	s = cairo_image_surface_create_for_data((unsigned char *)canvas, CAIRO_FORMAT_ARGB32, xres, yres, xres * 4);

	c = cairo_create(s);

	/* fill it with black */
	cairo_set_source_rgba(c, 0, 0, 0, 1);
	cairo_paint(c);

	cairo_identity_matrix(c);

	return 0;
}

static int write_image(const char *file)
{
	return write_png_file(file, xres, yres, canvas);
}

static int draw_planets()
{

	for (game_planet_vector_citer i = planets.begin(); i != planets.end(); i++) {
		const game_planet *p = *i;

		cairo_set_source_rgba(c, p->color[0], p->color[2], p->color[1], 1);

		cairo_arc(c, p->x * zoom, p->y * zoom, 1, 0, 3.14159*2);
		cairo_fill(c);
	}

	return 0;
}

#if 0
static int cairo_test()
{
	size_t len = sizeof(uint32_t) * xres * yres;
	uint32_t *ptr = (uint32_t*)malloc(len);

	s = cairo_image_surface_create_for_data((unsigned char *)ptr, CAIRO_FORMAT_ARGB32, xres, yres, xres * 4);
	printf("surface %p\n", s);
	printf("surface data %p\n", cairo_image_surface_get_data(s));
	printf("ptr %p\n", ptr);

	c = cairo_create(s);
	printf("cairo %p\n", c);

	/* fill it with black */
	cairo_set_source_rgba(c, 0, 0, 0, 1);
	cairo_paint(c);

	/* set color to white */
	cairo_set_source_rgba(c, 1, 1, 1, 1);

	//cairo_new_path(c);
	//cairo_identity_matrix(c);
	cairo_matrix_t m;
	cairo_matrix_init_scale(&m, xres, yres);
	cairo_set_matrix(c, &m);
	cairo_set_line_width(c, .01);
	cairo_move_to(c, 0, 0);
	cairo_line_to(c, .5, .5);
	//cairo_close_path(c);
	//cairo_paint(c);
	cairo_stroke(c);
	cairo_arc(c, .5, .5, .25, 0, 3.14159*2);
	cairo_fill(c);

	write_png_file("what.png", xres, yres, ptr);

	return 0;
}
#endif

static int load_db()
{
	enum {
		STATE_INITIAL,
		STATE_SKIP_TABLE,
		STATE_TABLE,
	} state = STATE_INITIAL;
	enum {
		NONE,
		USER,
		PLAYER,
		PLANET,
	} table = NONE;
	FILE *fp;
	char linebuf[1024]; // line buffer
	int count;

	fp = fopen("db/clean.out", "r");
	if (!fp) 
		return -1;

	while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
		count++;

		std::string line = linebuf;
		if (line.size() > 0 && line[line.size()-1] != '\n') {
			//printf("line too long\n");
			// this is a super long line, chew up bytes until we get to EOL
			char c;
			do {
				c = fgetc(fp);
			} while (c != '\n' && c != EOF);
		}

		boost::trim(line);

		switch (state) {
			case STATE_INITIAL: {
				char tablestr[256];
				if (sscanf(line.c_str(), "COPY %255s", tablestr) > 0) {
					printf("table start '%s'\n", tablestr);

					if (!strcmp(tablestr, "auth_user")) {
						table = USER;
					} else if (!strcmp(tablestr, "dominion_player")) {
						table = PLAYER;
					} else if (!strcmp(tablestr, "dominion_planet")) {
						table = PLANET;
					} else {
						table = NONE;
						state = STATE_SKIP_TABLE;
					}

					if (table != NONE) {
						state = STATE_TABLE;
						count = 0;
					}
				}
				break;
			}
			case STATE_SKIP_TABLE:
				if (line == "\\.") {
					printf("SKIP: done with table, %d lines skipped\n", count);
					state = STATE_INITIAL;
					break;
				}
				break;
			case STATE_TABLE: {
				if (line == "\\.") {
					printf("TABLE: done with table, %d lines parsed\n", count);
					state = STATE_INITIAL;
					break;
				}

				// tokenize the line
				std::vector<std::string> tokens;
				boost::split(tokens, line, boost::is_any_of("\t"));

				//printf("TABLE: %s", line.c_str());
				//printf("token count %d\n", tokens.size());
				switch (table) {
					case USER: {
						int id;

						if (tokens.size() < 2)
							break;

						id = atoi(tokens[0].c_str());
						//printf("id %d, name '%s'\n", id, tokens[1].c_str());
						break;
					}
					case PLAYER: {
						if (tokens.size() < 4)
							break;

						game_player *player = new game_player;

						player->user_id = atoi(tokens[1].c_str());
						std::string &color = tokens[3];

						color_to_float(player->color, color.c_str());

						players.push_back(player);

						//printf("user_id %d color '%s'\n", player->user_id, color.c_str());
						break;
					}
					case PLANET: {
						if (tokens.size() < 19)
							break;
						
						game_planet *planet = new game_planet;

						planet->id = atoi(tokens[0].c_str());
						planet->name = tokens[1];
						planet->owner_id = atoi(tokens[2].c_str());
						planet->sector_id = atoi(tokens[3].c_str());
						planet->x = atof(tokens[4].c_str());
						planet->y = atof(tokens[5].c_str());
						planet->r = atof(tokens[6].c_str());
						color_to_float(planet->color, atoi(tokens[7].c_str()));
						planet->sensor_range = atof(tokens[10].c_str());

						planets.push_back(planet);

						//printf("id %d name '%s' owner_id %d\n", planet->id, planet->name.c_str(), planet->owner_id);
						break;
					}
					default:
						;
				}
				break;
			}
		}
	}

	fclose(fp);

	return 0;
}

int main(int argc, char **argv)
{
	// deal with options
	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "help message")
		("xres", po::value<int>(&xres)->default_value(800), "x resolution")
		("yres", po::value<int>(&yres)->default_value(600), "y resolution")
		("zoom,z", po::value<int>(&zoom)->default_value(1), "zoom factor")
//		("cpus,c", po::value<int>(&cpus)->default_value(8), "number cpus")
//		("out,o", po::value<std::string>(&outfile)->default_value(std::string("out.ray")), "output file")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << std::endl;
		return 1;
	}

//	cairo_test();

	printf("loading database...\n");
	load_db();
	printf("done loading database.\n");

	// walk through some
	float maxx, maxy;

	maxx = maxy = 0.0;
	for (game_planet_vector_citer i = planets.begin(); i != planets.end(); i++) {
		const game_planet *p = *i;

		if (p->x > maxx)
			maxx = p->x;
		if (p->y > maxy)
			maxy = p->y;
	}
	printf("maxx %f maxy %f\n", maxx, maxy);

	// override xres/yres for now
	xres = maxx + 1;
	yres = maxy + 1;

	xres *= zoom;
	yres *= zoom;

	printf("xres %d yres %d\n", xres, yres);

	init_canvas();

	draw_planets();

	write_image("what.png");

	return 0;

}

