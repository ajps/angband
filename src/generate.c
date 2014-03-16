/*
 * File: generate.c
 * Purpose: Dungeon generation.
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2013 Erik Osheim, Nick McConnell
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cave.h"
#include "dungeon.h"
#include "math.h"
#include "files.h"
#include "game-event.h"
#include "generate.h"
#include "init.h"
#include "mon-make.h"
#include "mon-spell.h"
#include "monster.h"
#include "obj-util.h"
#include "object.h"
#include "parser.h"
#include "trap.h"
#include "z-queue.h"
#include "z-type.h"

/*
 * Array of pit types
 */
struct pit_profile *pit_info;
struct vault *vaults;


/**
 * See the "vault.txt" file for more on vault generation.
 * See the "room_template.txt" file for more room templates.
 *
 * In this file, we use the SQUARE_WALL flags to cave->info, which should only
 * be applied to granite.  SQUARE_WALL_SOLID indicates the wall should not be
 * tunnelled; SQUARE_WALL_INNER is the inward-facing wall of a room;
 * SQUARE_WALL_OUTER is the outer wall of a room.
 *
 * We use SQUARE_WALL_SOLID to prevent multiple corridors from piercing a wall
 * in two adjacent locations, which would be messy, and SQUARE_WALL_OUTER
 * to indicate which walls surround rooms, and may thus be pierced by corridors
 * entering or leaving the room.
 *
 * Note that a tunnel which attempts to leave a room near the edge of the
 * dungeon in a direction toward that edge will cause "silly" wall piercings,
 * but will have no permanently incorrect effects, as long as the tunnel can
 * eventually exit from another side. And note that the wall may not come back
 * into the room by the hole it left through, so it must bend to the left or
 * right and then optionally re-enter the room (at least 2 grids away). This is
 * not a problem since every room that is large enough to block the passage of
 * tunnels is also large enough to allow the tunnel to pierce the room itself
 * several times.
 *
 * Note that no two corridors may enter a room through adjacent grids, they
 * must either share an entryway or else use entryways at least two grids
 * apart. This prevents large (or "silly") doorways.
 *
 * Traditionally, to create rooms in the dungeon, it was divided up into
 * "blocks" of 11x11 grids each, and all rooms were required to occupy a
 * rectangular group of blocks.  As long as each room type reserved a
 * sufficient number of blocks, the room building routines would not need to
 * check bounds. Note that in classic generation most of the normal rooms
 * actually only use 23x11 grids, and so reserve 33x11 grids.
 *
 * Note that a lot of the original motivation for the block system was the
 * fact that there was only one size of map available, 22x66 grids, and the
 * dungeon level was divided up into nine of these in three rows of three.
 * Now that the map can be resized and enlarged, and dungeon levels themselves
 * can be different sizes, much of this original motivation has gone.  Blocks
 * can still be used, but different cave profiles can set their own block
 * sizes.  The classic generation method still uses the traditional blocks; the
 * main motivation for using blocks now is for the aesthetic effect of placing
 * rooms on a grid.
 */

/* name function height width min-depth pit? rarity %cutoff */
struct room_profile classic_rooms[] = {
    /* greater vaults only have rarity 1 but they have other checks */
    {"greater vault", build_greater_vault, 44, 66, 35, FALSE, 0, 100},

    /* very rare rooms (rarity=2) */
    {"monster pit", build_pit, 11, 33, 5, TRUE, 2, 8},
    {"monster nest", build_nest, 11, 33, 5, TRUE, 2, 16},
    {"medium vault", build_medium_vault, 22, 33, 30, FALSE, 2, 38},
    {"lesser vault", build_lesser_vault, 22, 33, 20, FALSE, 2, 55},


    /* unusual rooms (rarity=1) */
    {"large room", build_large, 11, 33, 3, FALSE, 1, 15},
    {"crossed room", build_crossed, 11, 33, 3, FALSE, 1, 35},
    {"circular room", build_circular, 22, 22, 1, FALSE, 1, 50},
    {"overlap room", build_overlap, 11, 33, 1, FALSE, 1, 70},
    {"room template", build_template, 11, 33, 5, FALSE, 1, 100},

    /* normal rooms */
    {"simple room", build_simple, 11, 33, 1, FALSE, 0, 100}
};

/* name function height width min-depth pit? rarity %cutoff */
struct room_profile modified_rooms[] = {
    /* really big rooms have rarity 0 but they have other checks */
    {"greater vault", build_greater_vault, 44, 66, 35, FALSE, 0, 100},
	{"huge room", build_huge, 44, 66, 40, FALSE, 0, 100},

    /* very rare rooms (rarity=2) */
	{"room of chambers", build_room_of_chambers, 44, 66, 10, FALSE, 2, 4},
    {"monster pit", build_pit, 11, 33, 5, TRUE, 2, 12},
    {"monster nest", build_nest, 11, 33, 5, TRUE, 2, 20},
    {"medium vault", build_medium_vault, 22, 33, 30, FALSE, 2, 40},
    {"lesser vault", build_lesser_vault, 22, 33, 20, FALSE, 2, 60},


    /* unusual rooms (rarity=1) */
	{"interesting room", build_interesting, 44, 55, 0, FALSE, 1, 10},
    {"large room", build_large, 11, 33, 3, FALSE, 1, 25},
    {"crossed room", build_crossed, 11, 33, 3, FALSE, 1, 40},
    {"circular room", build_circular, 22, 22, 1, FALSE, 1, 55},
    {"overlap room", build_overlap, 11, 33, 1, FALSE, 1, 70},
    {"room template", build_template, 11, 33, 5, FALSE, 1, 100},

    /* normal rooms */
    {"simple room", build_simple, 11, 33, 1, FALSE, 0, 100}
};

/**
 * Profiles used for generating dungeon levels.
 */
struct cave_profile cave_profiles[] = {
	{
		"town", town_gen, 1, 00, 200, 0, 0,

		/* tunnels -- not applicable */
		{"tunnel-null", 0, 0, 0, 0, 0},

		/* streamers -- not applicable */
		{"streamer-null", 0, 0, 0, 0, 0, 0},

		/* room_profiles -- not applicable */
		NULL,

		/* cutoff -- not applicable */
		-1
	},
	/* Points to note about this particular profile:
	 * - block size is 1, which essentially means no blocks
	 * - more comments at the definition of modified_gen in gen-cave.c */
	{
		/* name builder block dun_rooms dun_unusual max_rarity #room_profiles */
		"modified", modified_gen, 1, 50, 250, 2, N_ELEMENTS(modified_rooms),

		/* name rnd chg con pen jct */
		{"tunnel-classic", 10, 30, 15, 25, 90},

		/* name den rng mag mc qua qc */
		{"streamer-classic", 5, 2, 3, 90, 2, 40},

		/* room_profiles */
		modified_rooms,

		/* cutoff  -- not applicable because profile currently unused */
		-1
	},
	{
		"hard_centre", hard_centre_gen, 1, 0, 200, 0, 0,

		/* tunnels -- not applicable */
		{"tunnel-null", 0, 0, 0, 0, 0},

		/* streamers -- not applicable */
		{"streamer-null", 0, 0, 0, 0, 0, 0},

		/* room_profiles -- not applicable */
		NULL,

		/* cutoff -- unused because of special labyrinth_check  */
		-1
	},
	{
		"labyrinth", labyrinth_gen, 1, 0, 200, 0, 0,

		/* tunnels -- not applicable */
		{"tunnel-null", 0, 0, 0, 0, 0},

		/* streamers -- not applicable */
		{"streamer-null", 0, 0, 0, 0, 0, 0},

		/* room_profiles -- not applicable */
		NULL,

		/* cutoff -- unused because of special labyrinth_check  */
		-1
	},
    {
		"cavern", cavern_gen, 1, 0, 200, 0, 0,

		/* tunnels -- not applicable */
		{"tunnel-null", 0, 0, 0, 0, 0},

		/* streamers -- not applicable */
		{"streamer-null", 0, 0, 0, 0, 0, 0},

		/* room_profiles -- not applicable */
		NULL,

		/* cutoff */
		10
    },
    {
		/* name builder block dun_rooms dun_unusual max_rarity n_room_profiles */
		"classic", classic_gen, 11, 50, 200, 2, N_ELEMENTS(classic_rooms),

		/* name rnd chg con pen jct */
		{"tunnel-classic", 10, 30, 15, 25, 90},

		/* name den rng mag mc qua qc */
		{"streamer-classic", 5, 2, 3, 90, 2, 40},

		/* room_profiles */
		classic_rooms,

		/* cutoff */
		100
    }
};


/* Parsing functions for room_template.txt */
static enum parser_error parse_room_n(struct parser *p) {
    struct room_template *h = parser_priv(p);
    struct room_template *t = mem_zalloc(sizeof *t);

    t->tidx = parser_getuint(p, "index");
    t->name = string_make(parser_getstr(p, "name"));
    t->next = h;
    parser_setpriv(p, t);
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_room_x(struct parser *p) {
    struct room_template *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->typ = parser_getuint(p, "type");
    t->rat = parser_getint(p, "rating");
    t->hgt = parser_getuint(p, "height");
    t->wid = parser_getuint(p, "width");
    t->dor = parser_getuint(p, "doors");
    t->tval = parser_getuint(p, "tval");

    return PARSE_ERROR_NONE;
}

static enum parser_error parse_room_d(struct parser *p) {
    struct room_template *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->text = string_append(t->text, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static struct parser *init_parse_room(void) {
    struct parser *p = parser_new();
    parser_setpriv(p, NULL);
    parser_reg(p, "V sym version", ignored);
    parser_reg(p, "N uint index str name", parse_room_n);
    parser_reg(p, "X uint type int rating uint height uint width uint doors uint tval", parse_room_x);
    parser_reg(p, "D str text", parse_room_d);
    return p;
}

static errr run_parse_room(struct parser *p) {
    return parse_file(p, "room_template");
}

static errr finish_parse_room(struct parser *p) {
    room_templates = parser_priv(p);
    parser_destroy(p);
    return 0;
}

static void cleanup_room(void)
{
    struct room_template *t, *next;
    for (t = room_templates; t; t = next) {
		next = t->next;
		mem_free(t->name);
		mem_free(t->text);
		mem_free(t);
    }
}

static struct file_parser room_parser = {
    "room_template",
    init_parse_room,
    run_parse_room,
    finish_parse_room,
    cleanup_room
};

static void run_room_parser(void) {
    event_signal_string(EVENT_INITSTATUS, "Initializing arrays... (room templates)");
    if (run_parser(&room_parser))
		quit("Cannot initialize room templates");
}




/**
 * Clear the dungeon, ready for generation to begin.
 */
static void cave_clear(struct cave *c, struct player *p) {
    int i, x, y;

    wipe_o_list(c);
    wipe_mon_list(c, p);
	wipe_trap_list(c);


    /* Clear flags and flow information. */
    for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			/* Erase features */
			c->feat[y][x] = 0;

			/* Erase flags */
			sqinfo_wipe(c->info[y][x]);

			/* Erase flow */
			c->cost[y][x] = 0;
			c->when[y][x] = 0;

			/* Erase monsters/player */
			c->m_idx[y][x] = 0;

			/* Erase items */
			c->o_idx[y][x] = 0;
		}
    }

	/* Wipe feature counts */
	for (i = 0; i < z_info->f_max + 1; i++)
		c->feat_count[i] = 0;

    /* Unset the player's coordinates */
    p->px = p->py = 0;

    /* Nothing special here yet */
    c->good_item = FALSE;

    /* Nothing good here yet */
    c->mon_rating = 0;
    c->obj_rating = 0;
}

/**
 * Place hidden squares that will be used to generate feeling
 */
static void place_feeling(struct cave *c)
{
    int y,x,i,j;
    int tries = 500;
	
    for (i = 0; i < FEELING_TOTAL; i++) {
		for (j = 0; j < tries; j++) {
			/* Pick a random dungeon coordinate */
			y = randint0(c->height);
			x = randint0(c->width);

			/* Check to see if it is not a wall */
			if (square_iswall(c, y, x))
				continue;

			/* Check to see if it is already marked */
			if (square_isfeel(c, y, x))
				continue;

			/* Set the cave square appropriately */
			sqinfo_on(c->info[y][x], SQUARE_FEEL);
			
			break;
		}
    }

    /* Reset number of feeling squares */
    c->feeling_squares = 0;
}


/**
 * Calculate the level feeling for objects.
 */
static int calc_obj_feeling(struct cave *c)
{
    u32b x;

    /* Town gets no feeling */
    if (c->depth == 0) return 0;

    /* Artifacts trigger a special feeling when preserve=no */
    if (c->good_item && OPT(birth_no_preserve)) return 10;

    /* Check the loot adjusted for depth */
    x = c->obj_rating / c->depth;

    /* Apply a minimum feeling if there's an artifact on the level */
    if (c->good_item && x < 64001) return 60;

    if (x > 16000000) return 20;
    if (x > 4000000) return 30;
    if (x > 1000000) return 40;
    if (x > 250000) return 50;
    if (x > 64000) return 60;
    if (x > 16000) return 70;
    if (x > 4000) return 80;
    if (x > 1000) return 90;
    return 100;
}

/**
 * Calculate the level feeling for monsters.
 */
static int calc_mon_feeling(struct cave *c)
{
    u32b x;

    /* Town gets no feeling */
    if (c->depth == 0) return 0;

    /* Check the monster power adjusted for depth */
    x = c->mon_rating / (c->depth * c->depth);

    if (x > 7000) return 1;
    if (x > 4500) return 2;
    if (x > 2500) return 3;
    if (x > 1500) return 4;
    if (x > 800) return 5;
    if (x > 400) return 6;
    if (x > 150) return 7;
    if (x > 50) return 8;
    return 9;
}

/**
 * Do d_m's prime check for labyrinths
 */
bool labyrinth_check(struct cave *c)
{
    /* There's a base 2 in 100 to accept the labyrinth */
    int chance = 2;

    /* If we're too shallow then don't do it */
    if (c->depth < 13) return FALSE;

    /* Don't try this on quest levels, kids... */
    if (is_quest(c->depth)) return FALSE;

    /* Certain numbers increase the chance of having a labyrinth */
    if (c->depth % 3 == 0) chance += 1;
    if (c->depth % 5 == 0) chance += 1;
    if (c->depth % 7 == 0) chance += 1;
    if (c->depth % 11 == 0) chance += 1;
    if (c->depth % 13 == 0) chance += 1;

    /* Only generate the level if we pass a check */
    if (randint0(100) >= chance) return FALSE;

	/* Successfully ran the gauntlet! */
	return TRUE;
}

/**
 * Find a cave_profile by name
 */
const struct cave_profile *find_cave_profile(char *name)
{
	size_t i;

	for (i = 0; i < N_ELEMENTS(cave_profiles); i++) {
		const struct cave_profile *profile;

		profile = &cave_profiles[i];
		if (!strcmp(name, profile->name))
			return profile;
	}

	/* Not there */
	return NULL;
}

/**
 * Choose a cave profile
 */
const struct cave_profile *choose_profile(struct cave *c)
{
	const struct cave_profile *profile = NULL;

	/* A bit of a hack, but worth it for now NRM */
	if (player->noscore & NOSCORE_JUMPING) {
		char name[30];

		/* Cancel the query */
		player->noscore &= ~(NOSCORE_JUMPING);

		/* Ask debug players for the profile they want */
		if (get_string("Profile name (eg classic): ", name, sizeof(name)))
			profile = find_cave_profile(name);

		/* If no valid profile name given, fall through */
		if (profile) return profile;
	}

	/* Make the profile choice */
	if (c->depth == 0)
		profile = find_cave_profile("town");
	else if (is_quest(c->depth))
		/* Quest levels must be normal levels */
		profile = find_cave_profile("classic");
	else if (labyrinth_check(c))
		profile = find_cave_profile("labyrinth");
	else {
		int perc = randint0(100);
		size_t i;
		for (i = 0; i < N_ELEMENTS(cave_profiles); i++) {
			profile = &cave_profiles[i];
			if (profile->cutoff >= perc) break;
		}
	}

	/* Return the profile or fail horribly */
	if (profile)
		return profile;
	else
		quit("Failed to find cave profile!");

	return NULL;
}

/**
 * Generate a random level.
 *
 * Confusingly, this function also generate the town level (level 0).
 */
void cave_generate(struct cave *c, struct player *p) {
    const char *error = "no generation";
    int y, x, tries = 0;
	struct cave *chunk;

    assert(c);

    /* Start with dungeon-wide permanent rock */
	c->height = DUNGEON_HGT;
	c->width = DUNGEON_WID;
	cave_clear(c, p);
	fill_rectangle(c, 0, 0, DUNGEON_HGT - 1, DUNGEON_WID - 1, FEAT_PERM,
				   SQUARE_NONE);

	c->depth = p->depth;

	/* Generate */
	for (tries = 0; tries < 100 && error; tries++) {
		struct dun_data dun_body;

		error = NULL;

		/* Mark the dungeon as being unready (to avoid artifact loss, etc) */
		character_dungeon = FALSE;

		/* Allocate global data (will be freed when we leave the loop) */
		dun = &dun_body;

		/* Choose a profile and build the level */
		dun->profile = choose_profile(c);
		chunk = dun->profile->builder(p);
		if (!chunk) {
			error = "Failed to find builder";
			continue;
		}

		/* Ensure quest monsters */
		if (is_quest(chunk->depth)) {
			int i;
			for (i = 1; i < z_info->r_max; i++) {
				monster_race *r_ptr = &r_info[i];
				int y, x;
				
				/* The monster must be an unseen quest monster of this depth. */
				if (r_ptr->cur_num > 0) continue;
				if (!rf_has(r_ptr->flags, RF_QUESTOR)) continue;
				if (r_ptr->level != chunk->depth) continue;
	
				/* Pick a location and place the monster */
				find_empty(chunk, &y, &x);
				place_new_monster(chunk, y, x, r_ptr, TRUE, TRUE, ORIGIN_DROP);
			}
		}

		/* Clear generation flags. */
		for (y = 0; y < chunk->height; y++) {
			for (x = 0; x < chunk->width; x++) {
				sqinfo_off(chunk->info[y][x], SQUARE_WALL_INNER);
				sqinfo_off(chunk->info[y][x], SQUARE_WALL_OUTER);
				sqinfo_off(chunk->info[y][x], SQUARE_WALL_SOLID);
				sqinfo_off(chunk->info[y][x], SQUARE_MON_RESTRICT);
			}
		}

		/* Regenerate levels that overflow their maxima */
		if (cave_object_max(chunk) >= z_info->o_max)
			error = "too many objects";
		if (cave_monster_max(chunk) >= z_info->m_max)
			error = "too many monsters";

		if (error) ROOM_LOG("Generation restarted: %s.", error);
    }

    if (error) quit_fmt("cave_generate() failed 100 times!");

	/* Re-adjust cave size */
	c->height = chunk->height;
	c->width = chunk->width;

	/* Copy into the cave */
	if (!chunk_copy(c, chunk, 0, 0, 0, 0))
		quit_fmt("chunk_copy() level bounds failed!");

	/* Free it TODO make this process more robust */
	if (chunk_find(chunk)) chunk_list_remove(chunk->name);
	cave_free(chunk);

	/* Place dungeon squares to trigger feeling (not in town) */
	if (player->depth)
		place_feeling(c);

	/* Save the town */
	else if (!chunk_find_name("Town")) {
		struct cave *town = chunk_write(0, 0, TOWN_HGT, TOWN_WID, FALSE,
										FALSE, FALSE, TRUE);
		town->name = string_make("Town");
		chunk_list_add(town);
	}

	c->feeling = calc_obj_feeling(c) + calc_mon_feeling(c);

    /* The dungeon is ready */
    character_dungeon = TRUE;

    c->created_at = turn;
}



struct init_module generate_module = {
    .name = "generate",
    .init = run_room_parser,
    .cleanup = NULL
};
