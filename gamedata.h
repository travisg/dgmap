#pragma once

#include <string>
#include <list>
#include <map>

struct game_player {
	int user_id;
	float color[3];
};

typedef std::vector<game_player *> game_player_vector;
typedef std::vector<game_player *>::const_iterator game_player_vector_citer;
extern game_player_vector players;

struct game_planet {
	int id;
	std::string name;
	int owner_id;
	int sector_id;
	float x;
	float y;
	float r;
	float color[3];
	float sensor_range;
};

typedef std::vector<game_planet *> game_planet_vector;
typedef std::vector<game_planet *>::const_iterator game_planet_vector_citer;
extern game_planet_vector planets;

