/*
 * File: variable.c
 * Purpose: Various global variables
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "buildid.h"


/*
 * Hack -- Link a copyright message into the executable
 */
const char *copyright =
	"Copyright (c) 1987-2014 Angband contributors.\n"
	"\n"
	"This work is free software; you can redistribute it and/or modify it\n"
	"under the terms of either:\n"
	"\n"
	"a) the GNU General Public License as published by the Free Software\n"
	"   Foundation, version 2, or\n"
	"\n"
	"b) the Angband licence:\n"
	"   This software may be copied and distributed for educational, research,\n"
	"   and not for profit purposes provided that this copyright and statement\n"
	"   are included in all such copies.  Other copyrights may also apply.\n";

/*
 * Run-time arguments
 */
bool arg_wizard;			/* Command arg -- Request wizard mode */
bool arg_rebalance;			/* Command arg -- Rebalance monsters */
int arg_graphics;			/* Command arg -- Request graphics mode */
bool arg_graphics_nice;			/* Command arg -- Request nice graphics mode */

/*
 * Various things
 */

bool character_generated;	/* The character exists */
bool character_dungeon;		/* The character has a dungeon */
bool character_saved;		/* The character was just saved to a savefile */

s16b character_xtra;		/* Depth of the game in startup mode */

u32b seed_randart;		/* Hack -- consistent random artifacts */

u32b seed_flavor;		/* Hack -- consistent object colors */
u32b seed_town;			/* Hack -- consistent town layout */

s32b turn;				/* Current game turn */

int use_graphics;		/* The "graphics" mode is enabled */

s16b signal_count;		/* Hack -- Count interrupts */

bool msg_flag;			/* Player has pending message */

u32b inkey_scan;		/* See the "inkey()" function */
bool inkey_flag;		/* See the "inkey()" function */

s16b o_max = 1;			/* Number of allocated objects */
s16b o_cnt = 0;			/* Number of live objects */

/*
 * Buffer to hold the current savefile name
 */
char savefile[1024];

/*
 * Array[z_info->r_max] of monster lore
 */
monster_lore *l_list;

/*
 * Array[MAX_STORES] of stores
 */
struct store *stores;

/*
 * Flag to override which store is selected if in a knowledge menu
 */
int store_knowledge = STORE_NONE;

/*
 * Array[RANDNAME_NUM_TYPES][num_names] of random names
 */
const char *** name_sections;



/*** Player information ***/

/*
 * The player other record (static)
 */
static player_other player_other_body;

/*
 * Pointer to the player other record
 */
player_other *op_ptr = &player_other_body;

/*
 * The player info record (static)
 */
static player_type player_type_body;

/*
 * Pointer to the player info record
 */
player_type *player = &player_type_body;


/*
 * The vault generation arrays
 */
feature_type *f_info;
trap_kind *trap_info;

object_kind *k_info;
object_base *kb_info;

/*
 * The artifact arrays
 */
artifact_type *a_info;

/*
 * The ego-item arrays
 */
ego_item_type *e_info;

/*
 * The monster race arrays
 */
monster_race *r_info;
monster_base *rb_info;
monster_pain *pain_messages;

struct player_race *races;
struct player_class *classes;
struct vault *vaults;
struct object_kind *objkinds;

struct flavor *flavors;

/*
 * The spell arrays
 */
spell_type *s_info;

/*
 * The hints array
 */
struct hint *hints;

/*
 * Array of pit types
 */
struct pit_profile *pit_info;
 
/*
 * Hack -- The special Angband "System Suffix"
 * This variable is used to choose an appropriate "pref-xxx" file
 */
const char *ANGBAND_SYS = "xxx";

/*
 * Hack -- The special Angband "Graphics Suffix"
 * This variable is used to choose an appropriate "graf-xxx" file
 */
const char *ANGBAND_GRAF = "old";

/*
 * Various directories. These are no longer necessarily all subdirs of "lib"
 */
char *ANGBAND_DIR_APEX;
char *ANGBAND_DIR_EDIT;
char *ANGBAND_DIR_FILE;
char *ANGBAND_DIR_HELP;
char *ANGBAND_DIR_INFO;
char *ANGBAND_DIR_SAVE;
char *ANGBAND_DIR_PREF;
char *ANGBAND_DIR_USER;
char *ANGBAND_DIR_XTRA;

/* 
 * Various xtra/ subdirectories.
 */
char *ANGBAND_DIR_XTRA_FONT;
char *ANGBAND_DIR_XTRA_GRAF;
char *ANGBAND_DIR_XTRA_SOUND;
char *ANGBAND_DIR_XTRA_ICON;



/*
 * Use transparent tiles
 */
bool use_transparency = FALSE;


/*
 * Sound hook (for playing FX).
 */
void (*sound_hook)(int sound);

/* Delay in centiseconds before moving to allow another keypress */
/* Zero means normal instant movement. */
u16b lazymove_delay = 0;

