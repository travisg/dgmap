#include <cstdio>
#include <cstdlib>
#include <limits.h>
#include <cairo/cairo.h>

#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>

#include "gamedata.h"

namespace po = boost::program_options;

game_player_vector players;
game_planet_vector planets;

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

static int cairo_test()
{
	cairo_surface_t *s;

#define W 640
#define H 480

	size_t len = sizeof(uint32_t) * W * H;
	uint32_t *ptr = (uint32_t*)malloc(len);
	memset(ptr, 0, len);

	s = cairo_image_surface_create_for_data((unsigned char *)ptr, CAIRO_FORMAT_ARGB32, W, H, W * 4);
	printf("surface %p\n", s);
	printf("surface data %p\n", cairo_image_surface_get_data(s));
	printf("ptr %p\n", ptr);

	cairo_t *c = cairo_create(s);
	printf("cairo %p\n", c);

	cairo_set_source_rgba(c, 1, .5, .75, 1);

	//cairo_new_path(c);
	cairo_identity_matrix(c);
	cairo_set_line_width(c, 1);
	cairo_arc(c, W/2, H/2, H/2, 0, 1);
	//cairo_move_to(c, 0, 0);
	//cairo_line_to(c, W/2, H/2);
	//cairo_close_path(c);
	//cairo_paint(c);
	cairo_stroke(c);
	//cairo_fill(c);

	FILE *fp = fopen("image.dat", "w+");
	fwrite(ptr, len, 1, fp);
	fclose(fp);


	return 0;
}

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
//		("xres", po::value<int>(&xres)->default_value(800), "x resolution")
//		("yres", po::value<int>(&yres)->default_value(600), "y resolution")
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

	load_db();

	return 0;

}

