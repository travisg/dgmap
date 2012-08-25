#include <cstdio>
#include <cairo/cairo.h>

#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/thread.hpp>

namespace po = boost::program_options;

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

