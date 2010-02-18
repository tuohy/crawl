/*
 *  File:       abyss.cc
 *  Summary:    Misc abyss specific functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "abyss.h"

#include <cstdlib>
#include <algorithm>

#include "areas.h"
#include "artefact.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "makeitem.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "coord.h"
#include "mon-transit.h"
#include "player.h"
#include "dungeon.h"
#include "items.h"
#include "l_defs.h"
#include "lev-pand.h"
#include "los.h"
#include "random.h"
#include "religion.h"
#include "showsymb.h"
#include "stuff.h"
#include "env.h"
#include "spells3.h"
#include "terrain.h"
#ifdef USE_TILE
#include "tiledef-dngn.h"
#endif
#include "tiles.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "xom.h"

const coord_def abyss_center(45,35);

// If not_seen is true, don't place the feature where it can be seen from
// the centre.
static bool _place_feature_near(const coord_def &centre,
                                int radius,
                                dungeon_feature_type candidate,
                                dungeon_feature_type replacement,
                                int tries, bool not_seen = false)
{
    const int radius2 = radius * radius + 1;
    for (int i = 0; i < tries; ++i)
    {
        const coord_def &cp =
            centre + coord_def(random_range(-radius, radius),
                               random_range(-radius, radius));

        if (cp == centre || (cp - centre).abs() > radius2 || !in_bounds(cp))
            continue;

        if (not_seen && cell_see_cell(cp, centre))
            continue;

        if (grd(cp) == candidate)
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Placing %s at (%d,%d)",
                 dungeon_feature_name(replacement),
                 cp.x, cp.y);
#endif
            grd(cp) = replacement;
            return (true);
        }
    }
    return (false);
}

//#define DEBUG_ABYSS

// Public for abyss generation.
void generate_abyss()
{
    extern std::string dgn_Build_Method;

    dgn_Build_Method += " abyss";
    dgn_Layout_Type   = "abyss";

#ifdef DEBUG_ABYSS
    mprf(MSGCH_DIAGNOSTICS,
         "generate_abyss(); turn_on_level: %d", env.turns_on_level);
#endif

    for (rectangle_iterator ri(5); ri; ++ri)
    {
        grd(*ri) =
            static_cast<dungeon_feature_type>(
                random_choose_weighted(3000, DNGN_FLOOR,
                                        600, DNGN_ROCK_WALL,
                                        300, DNGN_STONE_WALL,
                                        100, DNGN_METAL_WALL,
                                          1, DNGN_CLOSED_DOOR,
                                          0));
    }


    // If we're starting out in the Abyss, make sure the starting grid is
    // an altar to Lugonu and there's an exit near-by.
    // Otherwise, we start out on floor and there's a chance there's an
    // altar near-by.
    if (you.char_direction == GDT_GAME_START)
    {
        grd(abyss_center) = DNGN_ALTAR_LUGONU;
        _place_feature_near( abyss_center, LOS_RADIUS + 2,
                             DNGN_FLOOR, DNGN_EXIT_ABYSS, 50, true );
    }
    else
    {
        grd(abyss_center) = DNGN_FLOOR;
        if (one_chance_in(5))
        {
            _place_feature_near( abyss_center, LOS_RADIUS,
                                 DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50 );
        }
    }
}

// Returns the roll to use to check if we want to create an abyssal rune.
static int _abyssal_rune_roll()
{
    if (you.attribute[ATTR_ABYSSAL_RUNES])
        return (-1);

    // The longer the player's hung around in the Abyss, the more
    // likely the rune. Never generate a new rune if the player
    // already found one, but make the Abyssal rune eligible for
    // generation again if the player loses it.

    // If the player leaves the Abyss turns_on_level resets to 0. So
    // hang in there if you want your Abyssal rune fix quick. :P

    // Worshippers of Lugonu with decent piety will attract the rune
    // to themselves.

    const bool lugonu_favoured =
        (you.religion == GOD_LUGONU && !player_under_penance()
         && you.piety > 120);

    const int cutoff = lugonu_favoured ? 50 : 500;
    const int scale = lugonu_favoured ? 10 : 40;

    const int odds =
        std::max(200 - std::max((env.turns_on_level - cutoff) / scale, 0), 6);
#ifdef DEBUG_ABYSS
    mprf(MSGCH_DIAGNOSTICS, "Abyssal rune odds: 1 in %d", odds);
#endif
    return (odds);
}

static void _generate_area(const coord_def& topleft,
                           const coord_def& bottomright, bool spatter = false)
{
    // Any rune on the floor prevents the abyssal rune from being generated.
    bool placed_abyssal_rune =
        find_floor_item(OBJ_MISCELLANY, MISC_RUNE_OF_ZOT);

#ifdef DEBUG_ABYSS
    mprf(MSGCH_DIAGNOSTICS,
         "_generate_area(). turns_on_level: %d, rune_on_floor: %s",
         env.turns_on_level, placed_abyssal_rune? "yes" : "no");
#endif

    const int abyssal_rune_roll = _abyssal_rune_roll();
    int items_placed = 0;
    const int thickness = random2(70) + 30;
    int thing_created;

    dungeon_feature_type replaced[5];

    // Nuke map.
    env.map_knowledge.init(map_cell());
    env.pgrid.init(0);

    // Generate level composition vector.
    for (int i = 0; i < 5; i++)
    {
        const int temp_rand = random2(10000);

        replaced[i] = ((temp_rand > 4926) ? DNGN_ROCK_WALL :    // 50.73%
                       (temp_rand > 2918) ? DNGN_STONE_WALL :   // 20.08%
                       (temp_rand > 2004) ? DNGN_METAL_WALL :   //  9.14%
                       (temp_rand > 1282) ? DNGN_LAVA :         //  7.22%
                       (temp_rand > 616)  ? DNGN_SHALLOW_WATER ://  6.66%
                       (temp_rand > 15)   ? DNGN_DEEP_WATER     //  6.01%
                                          : DNGN_CLOSED_DOOR);  //  0.16%
    }

    if (one_chance_in(3))
    {
        // Place some number of rooms.
        int rooms_to_do = 1 + random2(10);

        for (int i = 0; i < rooms_to_do; ++i)
        {
            // Pick the corners
            coord_def tl( 10 + random2(GXM - 20), 10 + random2(GYM - 20) );
            coord_def br( tl.x + 1 + random2(10), tl.y + 1 + random2(10) );

            if (one_chance_in(100))
                break;

            bool room_ok = true;

            // Check if the room is taken.
            for (rectangle_iterator ri(tl, br); ri && room_ok; ++ri)
                if (grd(*ri) != DNGN_UNSEEN)
                    room_ok = false;

            // Make the room.
            if (room_ok)
            {
                for (rectangle_iterator ri(tl,br); ri; ++ri)
                    grd(*ri) = DNGN_FLOOR;
            }
        }
    }

    // During game start, number and level of items mustn't be higher than
    // that on level 1.
    int num_items = 150, items_level = 51;
    if (you.char_direction == GDT_GAME_START)
    {
        num_items   = 3 + roll_dice( 3, 11 );
        items_level = 0;
    }

    for (rectangle_iterator ri(topleft, bottomright); ri; ++ri)
    {
        if (grd(*ri) == DNGN_UNSEEN && x_chance_in_y(thickness + 1, 100))
        {
            grd(*ri) = DNGN_FLOOR;

            if (items_placed < num_items && one_chance_in(200))
            {
                if (!placed_abyssal_rune && abyssal_rune_roll != -1
                    && you.char_direction != GDT_GAME_START
                    && one_chance_in(abyssal_rune_roll))
                {
                    thing_created = items(1, OBJ_MISCELLANY,
                                          MISC_RUNE_OF_ZOT, true, 51, 51);
                    placed_abyssal_rune = true;
#ifdef DEBUG_ABYSS
                    mpr("Placing an Abyssal rune.", MSGCH_DIAGNOSTICS);
#endif
                }
                else
                {
                    thing_created = items(1, OBJ_RANDOM, OBJ_RANDOM,
                                          true, items_level, 250);
                }

                move_item_to_grid( &thing_created, *ri );

                if (thing_created != NON_ITEM)
                    items_placed++;
            }
        }
    }

    int exits_wanted  = 0;
    int altars_wanted = 0;

    for (rectangle_iterator ri(topleft, bottomright); ri; ++ri)
    {
        if (grd(*ri) == DNGN_UNSEEN)
            grd(*ri) = replaced[random2(5)];

        if (one_chance_in(7500)) // place an exit
            exits_wanted++;

        // Don't place exit under items.
        if (exits_wanted > 0 && igrd(*ri) == NON_ITEM)
        {
            grd(*ri) = DNGN_EXIT_ABYSS;
            exits_wanted--;
#ifdef DEBUG_ABYSS
            mpr("Placing Abyss exit.", MSGCH_DIAGNOSTICS);
#endif
        }

        // Except for the altar on the starting position, don't place
        // any altars.
        if (you.char_direction != GDT_GAME_START)
        {
            if (one_chance_in(10000)) // Place an altar.
                altars_wanted++;

            // Don't place altars under items.
            if (altars_wanted > 0 && igrd(*ri) == NON_ITEM)
            {
                do
                {
                    grd(*ri) = static_cast<dungeon_feature_type>(
                        DNGN_ALTAR_ZIN + random2(NUM_GODS - 1));
                }
                while (grd(*ri) == DNGN_ALTAR_ZIN
                       || grd(*ri) == DNGN_ALTAR_SHINING_ONE
                       || grd(*ri) == DNGN_ALTAR_ELYVILON
                       || (jiyva_is_dead() ?
                              grd(*ri) == DNGN_ALTAR_JIYVA : false));

                // Lugonu has a flat 50% chance of corrupting the altar.
                if (coinflip())
                    grd(*ri) = DNGN_ALTAR_LUGONU;

                altars_wanted--;
#ifdef DEBUG_ABYSS
                mpr("Placing altar.", MSGCH_DIAGNOSTICS);
#endif
            }

            if (one_chance_in(10000))
            {
                // purely decorative
                grd(*ri) = DNGN_STONE_ARCH;
            }
        }
    }

    generate_random_blood_spatter_on_level();

    setup_environment_effects();
}

static int _abyss_exit_nearness()
{
    int nearness = INFINITE_DISTANCE;

    // is_terrain_known() doesn't work on unmappable levels because
    // mapping flags are not set on such levels.
    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
        if (grd(*ri) == DNGN_EXIT_ABYSS && get_screen_glyph(*ri) != ' ')
            nearness = std::min(nearness, grid_distance(you.pos(), *ri));

    return (nearness);
}

static int _abyss_rune_nearness()
{
    int nearness = INFINITE_DISTANCE;

    // See above comment about is_terrain_known().
    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
    {
        if (get_screen_glyph(*ri) != ' ')
        {
            for (stack_iterator si(*ri); si; ++si)
                if (is_rune(*si) && si->plus == RUNE_ABYSSAL)
                    nearness = std::min(nearness, grid_distance(you.pos(),*ri));
        }
    }
    return (nearness);
}

static int exit_was_near;
static int rune_was_near;

static void _xom_check_nearness_setup()
{
    exit_was_near = _abyss_exit_nearness();
    rune_was_near = _abyss_rune_nearness();
}

// If the player was almost to the exit when it disappeared, Xom is
// extremely amused.  He's also extremely amused if the player winds
// up right next to an exit when there wasn't one there before.  The
// same applies to Abyssal runes.
static void _xom_check_nearness()
{
    // Update known terrain
    viewwindow(false);

    int exit_is_near = _abyss_exit_nearness();
    int rune_is_near = _abyss_rune_nearness();

    if (exit_was_near < INFINITE_DISTANCE
            && exit_is_near == INFINITE_DISTANCE
        || rune_was_near < INFINITE_DISTANCE
            && rune_is_near == INFINITE_DISTANCE
            && you.attribute[ATTR_ABYSSAL_RUNES] == 0)
    {
        xom_is_stimulated(255, "Xom snickers loudly.", true);
    }

    if (rune_was_near == INFINITE_DISTANCE
            && rune_is_near < INFINITE_DISTANCE
            && you.attribute[ATTR_ABYSSAL_RUNES] == 0
        || exit_was_near == INFINITE_DISTANCE
            && exit_is_near < INFINITE_DISTANCE)
    {
        xom_is_stimulated(255);
    }
}

static void _abyss_lose_monster(monsters &mons)
{
    if (mons.needs_transit())
        mons.set_transit( level_id(LEVEL_ABYSS) );

    mons.destroy_inventory();
    mons.reset();
}

#define LOS_DIAMETER (LOS_RADIUS * 2 + 1)

void area_shift(void)
{
#ifdef DEBUG_ABYSS
    mprf(MSGCH_DIAGNOSTICS, "area_shift() - player at pos (%d, %d)",
         you.pos().x, you.pos().y);
#endif

    // Preserve floor props around the player, primarily so that
    // blood-splatter doesn't appear out of nowhere when doing an
    // area shift.
    //
    // Also shift sanctuary center if it's close.
    bool      sanct_shifted = false;
    coord_def sanct_pos;
    FixedArray<unsigned short, LOS_DIAMETER, LOS_DIAMETER> fprops;
    const coord_def los_delta(LOS_RADIUS, LOS_RADIUS);

    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
    {
        fprops(you.pos() - *ri + los_delta) = env.pgrid(*ri);
        if (env.sanctuary_pos == *ri && env.sanctuary_time > 0)
        {
            sanct_pos     = *ri - you.pos();
            sanct_shifted = true;
        }
    }

    // If sanctuary centre is outside of preserved area then just get
    // rid of it.
    if (env.sanctuary_time > 0 && !sanct_shifted)
        remove_sanctuary(false);

    _xom_check_nearness_setup();

    // Remove non-nearby monsters.
    for (monster_iterator mi; mi; ++mi)
    {
        if (grid_distance(mi->pos(), you.pos()) > 10)
            _abyss_lose_monster(**mi);
    }

    for (rectangle_iterator ri(5); ri; ++ri)
    {
        // Don't modify terrain by player.
        if (grid_distance(*ri, you.pos()) <= 10)
            continue;

        // Nuke terrain otherwise.
        grd(*ri) = DNGN_UNSEEN;

        // Nuke items.
#ifdef DEBUG_ABYSS
        if (igrd(*ri) != NON_ITEM)
        {
            const coord_def &p(*ri);
            mprf(MSGCH_DIAGNOSTICS, "Nuke item stack at (%d, %d)", p.x, p.y);
        }
#endif
        lose_item_stack( *ri );

        if (monsters* m = monster_at(*ri))
            _abyss_lose_monster(*m);
    }

    // Shift all monsters and items to new area.
    for (radius_iterator ri(you.pos(), 10, true, false); ri; ++ri)
    {
        const coord_def newpos = abyss_center + *ri - you.pos();

        // Move terrain.
        grd(newpos) = grd(*ri);

        // Move item.
#ifdef DEBUG_ABYSS
        if (igrd(*ri) != NON_ITEM)
        {
            mprf(MSGCH_DIAGNOSTICS,
                 "Move item stack from (%d, %d) to (%d, %d)",
                 ri->x, ri->y, newpos.x, newpos.y);
        }
#endif
        move_item_stack_to_grid(*ri, newpos);

        // Move monster.
        if (monster_at(*ri))
        {
            menv[mgrd(*ri)].moveto(newpos);
            mgrd(newpos) = mgrd(*ri);
            mgrd(*ri) = NON_MONSTER;
        }

        // Move cloud,
        if (env.cgrid(*ri) != EMPTY_CLOUD)
            move_cloud( env.cgrid(*ri), newpos );
    }

    for (int i = 0; i < MAX_CLOUDS; i++)
    {
        if (env.cloud[i].type == CLOUD_NONE)
            continue;

        if (grid_distance(abyss_center, env.cloud[i].pos) > 10)
            delete_cloud( i );
    }

    you.shiftto(abyss_center);

    _generate_area(coord_def(MAPGEN_BORDER, MAPGEN_BORDER),
                   coord_def(GXM - MAPGEN_BORDER, GYM - MAPGEN_BORDER), true);

    _xom_check_nearness();

    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
        env.pgrid(*ri) = fprops(you.pos() - *ri + los_delta);

    if (sanct_shifted)
        env.sanctuary_pos = sanct_pos + you.pos();

    // Place some number of monsters.
    mgen_data mons;
    mons.level_type = LEVEL_ABYSS;
    mons.proximity  = PROX_AWAY_FROM_PLAYER;

    for (unsigned int mcount = 0; mcount < 15; mcount++)
        mons_place(mons);

    // And allow monsters in transit another chance to return.
    place_transiting_monsters();
    place_transiting_items();
}

void save_abyss_uniques()
{
    for (monster_iterator mi; mi; ++mi)
         if (mi->needs_transit())
            mi->set_transit(level_id(LEVEL_ABYSS));
}

void abyss_teleport( bool new_area )
{
    _xom_check_nearness_setup();

    if (!new_area)
    {
        coord_def newspot;
        bool found = false;
        // Try to find a good spot within the shift zone.
        for (int i = 0; i < 100 && !found; i++)
        {
            newspot.x = 16 + random2( GXM - 32 );
            newspot.y = 16 + random2( GYM - 32 );

            if ((grd(newspot) == DNGN_FLOOR
                    || grd(newspot) == DNGN_SHALLOW_WATER)
                && !monster_at(newspot)
                && env.cgrid(newspot) == EMPTY_CLOUD)
            {
                found = true;
            }
        }

        if (found)
        {
#ifdef DEBUG_ABYSS
            mpr("Non-new area Abyss teleport.", MSGCH_DIAGNOSTICS);
#endif
            you.moveto(newspot);
            _xom_check_nearness();
            return;
        }
    }

    remove_sanctuary(false);

#ifdef DEBUG_ABYSS
    mpr("New area Abyss teleport.", MSGCH_DIAGNOSTICS);
#endif

    // Teleport to a new area of the abyss.

    // Get new monsters and colours.
    init_pandemonium();
#ifdef USE_TILE
    tile_init_flavour();
#endif

    for (monster_iterator mi; mi; ++mi)
        _abyss_lose_monster(**mi);

    // Orbs and fixed artefacts are marked as "lost in the abyss".
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        if (mitm[i].is_valid())
        {
            item_was_lost( mitm[i] );
            destroy_item( i );
        }
    }

    for (int i = 0; i < MAX_CLOUDS; i++)
        delete_cloud( i );

    for (rectangle_iterator ri(10); ri; ++ri)
    {
        grd(*ri)       = DNGN_UNSEEN;  // So generate_area will pick it up.
        igrd(*ri)      = NON_ITEM;
        mgrd(*ri)      = NON_MONSTER;
        env.cgrid(*ri) = EMPTY_CLOUD;
    }

    ASSERT( env.cloud_no == 0 );

    you.moveto(abyss_center);

    _generate_area(coord_def(MAPGEN_BORDER, MAPGEN_BORDER),
                   coord_def(GXM - MAPGEN_BORDER, GYM - MAPGEN_BORDER), true);

    _xom_check_nearness();

    grd(you.pos()) = DNGN_FLOOR;
    if (one_chance_in(5))
    {
        _place_feature_near( you.pos(), LOS_RADIUS,
                             DNGN_FLOOR, DNGN_ALTAR_LUGONU, 50 );
    }

    place_transiting_monsters();
    place_transiting_items();
}

//////////////////////////////////////////////////////////////////////////////
// Abyss effects in other levels, courtesy Lugonu.

static void _place_corruption_seed(const coord_def &pos, int duration)
{
    env.markers.add(new map_corruption_marker(pos, duration));
}

static void _initialise_level_corrupt_seeds(int power)
{
    const int low = power * 40 / 100, high = power * 140 / 100;
    const int nseeds = random_range(-1, std::min(2 + power / 110, 4), 2);

    const int aux_seed_radius = 4;

    dprf("Placing %d corruption seeds (power: %d)", nseeds, power);

    // The corruption centered on the player is free.
    _place_corruption_seed(you.pos(), high + 300);

    for (int i = 0; i < nseeds; ++i)
    {
        coord_def where;
        int tries = 100;
        while (tries-- > 0)
        {
            where = dgn_random_point_from(you.pos(), aux_seed_radius, 2);
            if (grd(where) == DNGN_FLOOR
                && !env.markers.find(where, MAT_ANY))
            {
                break;
            }
            where.reset();
        }

        if (!where.origin())
            _place_corruption_seed(where, random_range(low, high, 2) + 300);
    }
}

static bool _spawn_corrupted_servant_near(const coord_def &pos)
{
    const beh_type beh =
        one_chance_in(5 + you.skills[SK_INVOCATIONS] / 4) ? BEH_HOSTILE
        : BEH_NEUTRAL;

    // [ds] No longer summon hostiles.
    if (beh == BEH_HOSTILE)
        return false;

    // Thirty tries for a place.
    for (int i = 0; i < 30; ++i)
    {
        const coord_def p( pos.x + random2avg(4, 3) + random2(3),
                           pos.y + random2avg(4, 3) + random2(3) );
        if (!in_bounds(p) || actor_at(p)
            || !feat_compatible(DNGN_FLOOR, grd(p)))
        {
            continue;
        }

        // Got a place, summon the beast.
        monster_type mons = pick_random_monster(level_id(LEVEL_ABYSS));
        if (mons == MONS_PROGRAM_BUG)
            return (false);

        mgen_data mg(mons, beh, 0, 5, 0, p);
        mg.non_actor_summoner = "Lugonu's corruption";

        const int mid = create_monster(mg);
        return !invalid_monster_index(mid);
    }

    return (false);
}

static void _apply_corruption_effect( map_marker *marker, int duration)
{
    if (!duration)
        return;

    map_corruption_marker *cmark = dynamic_cast<map_corruption_marker*>(marker);
    if (cmark->duration < 1)
        return;

    const coord_def center = cmark->pos;
    const int neffects = std::max(div_rand_round(duration, 5), 1);

    for (int i = 0; i < neffects; ++i)
    {
        if (x_chance_in_y(cmark->duration, 4000)
            && !_spawn_corrupted_servant_near(cmark->pos))
        {
            break;
        }
    }
    cmark->duration -= duration;
}

void run_corruption_effects(int duration)
{
    std::vector<map_marker*> markers =
        env.markers.get_all(MAT_CORRUPTION_NEXUS);

    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_marker *mark = markers[i];
        if (mark->get_type() != MAT_CORRUPTION_NEXUS)
            continue;

        _apply_corruption_effect(mark, duration);
    }
}

static bool _is_grid_corruptible(const coord_def &c)
{
    if (c == you.pos())
        return (false);

    const dungeon_feature_type feat = grd(c);

    // Stairs and portals cannot be corrupted.
    if (feat_stair_direction(feat) != CMD_NO_CMD)
        return (false);

    switch (feat)
    {
    case DNGN_PERMAROCK_WALL:
    case DNGN_GREEN_CRYSTAL_WALL:
        return (false);

    case DNGN_METAL_WALL:
        return (one_chance_in(5));

    case DNGN_STONE_WALL:
        return (one_chance_in(3));

    case DNGN_ROCK_WALL:
        return (!one_chance_in(3));

    default:
        return (true);
    }
}

// Returns true if the square has <= 4 traversable neighbours.
static bool _is_crowded_square(const coord_def &c)
{
    int neighbours = 0;
    for (int xi = -1; xi <= 1; ++xi)
        for (int yi = -1; yi <= 1; ++yi)
        {
            if (!xi && !yi)
                continue;

            const coord_def n(c.x + xi, c.y + yi);
            if (!in_bounds(n) || !feat_is_traversable(grd(n)))
                continue;

            if (++neighbours > 4)
                return (false);
        }

    return (true);
}

// Returns true if the square has all opaque neighbours.
static bool _is_sealed_square(const coord_def &c)
{
    for (adjacent_iterator ai(c); ai; ++ai)
        if ( !feat_is_opaque(grd(*ai)) )
            return (false);

    return (true);
}

static void _corrupt_square(const crawl_environment &oenv, const coord_def &c)
{
    // To prevent the destruction of, say, branch entries.
    bool preserve_feat = true;
    dungeon_feature_type feat = DNGN_UNSEEN;
    if (feat_altar_god(grd(c)) != GOD_NO_GOD)
    {
        // altars may be safely overwritten, ha!
        preserve_feat = false;
        if (!one_chance_in(3))
            feat = DNGN_ALTAR_LUGONU;
    }
    else
        feat = oenv.grid(c);

    if (feat_is_trap(feat, true)
        || feat == DNGN_SECRET_DOOR || feat == DNGN_UNSEEN)
    {
        return;
    }

    if (feat_is_traversable(grd(c)) && !feat_is_traversable(feat)
        && _is_crowded_square(c))
    {
        return;
    }

    if (!feat_is_traversable(grd(c)) && feat_is_traversable(feat) && _is_sealed_square(c))
        return;

    // What's this supposed to achieve? (jpeg)
    // I mean, won't exits from the Abyss only turn up in the Abyss itself?
    if (feat == DNGN_EXIT_ABYSS)
        feat = DNGN_ENTER_ABYSS;

    dungeon_terrain_changed(c, feat, preserve_feat, true, true);
    if (feat == DNGN_ROCK_WALL)
        env.grid_colours(c) = oenv.rock_colour;
    else if (feat == DNGN_FLOOR)
        env.grid_colours(c) = oenv.floor_colour;

#ifdef USE_TILE
    if (feat == DNGN_ROCK_WALL)
    {
        env.tile_flv(c).wall = TILE_WALL_UNDEAD
            + random2(tile_dngn_count(TILE_WALL_UNDEAD));
    }
    else if (feat == DNGN_FLOOR)
    {
        env.tile_flv(c).floor = TILE_FLOOR_NERVES
            + random2(tile_dngn_count(TILE_FLOOR_NERVES));
    }
#endif
}

static void _corrupt_level_features(const crawl_environment &oenv)
{
    std::vector<coord_def> corrupt_seeds;
    std::vector<map_marker*> corrupt_markers =
        env.markers.get_all(MAT_CORRUPTION_NEXUS);

    for (int i = 0, size = corrupt_markers.size(); i < size; ++i)
        corrupt_seeds.push_back(corrupt_markers[i]->pos);

    for (rectangle_iterator ri(MAPGEN_BORDER); ri; ++ri)
    {
        int idistance2 = GXM * GXM + GYM * GYM;
        for (int i = 0, size = corrupt_seeds.size(); i < size; ++i)
        {
            const int idist2 = (*ri - corrupt_seeds[i]).abs();
            if (idist2 < idistance2)
                idistance2 = idist2;
        }

        const int ground_zero_radius2 = 7;

        // Corruption odds are 100% within about 2 squares, decaying to 30%
        // at LOS range (radius 8). Even if the corruption roll is made,
        // the feature still gets a chance to resist if it's a wall.
        const int corrupt_perc_chance =
            idistance2 <= ground_zero_radius2 ? 100 :
            std::max(1, 100 - (idistance2 - ground_zero_radius2) * 70 / 57);

        if (random2(100) < corrupt_perc_chance && _is_grid_corruptible(*ri))
            _corrupt_square(oenv, *ri);
    }
}

static bool _is_level_corrupted()
{
    if (you.level_type == LEVEL_ABYSS)
        return (true);

    return (!!env.markers.find(MAT_CORRUPTION_NEXUS));
}

bool is_level_incorruptible()
{
    if (_is_level_corrupted())
    {
        mpr("This place is already infused with evil and corruption.");
        return (true);
    }

    return (false);
}

static void _corrupt_choose_colours()
{
    int colour = BLACK;
    do
        colour = random_uncommon_colour();
    while (colour == env.rock_colour || colour == LIGHTGREY || colour == WHITE);
    env.rock_colour = colour;

    do
        colour = random_uncommon_colour();
    while (colour == env.floor_colour || colour == LIGHTGREY
           || colour == WHITE);
    env.floor_colour = colour;
}

bool lugonu_corrupt_level(int power)
{
    if (is_level_incorruptible())
        return (false);

    mpr("Lugonu's Hand of Corruption reaches out!", MSGCH_GOD);

    flash_view(MAGENTA);

    _initialise_level_corrupt_seeds(power);

    std::auto_ptr<crawl_environment> backup(new crawl_environment(env));
    generate_abyss();
    _generate_area(coord_def(MAPGEN_BORDER, MAPGEN_BORDER),
                   coord_def(GXM - MAPGEN_BORDER, GYM - MAPGEN_BORDER), false);

    _corrupt_choose_colours();

    std::auto_ptr<crawl_environment> abyssal(new crawl_environment(env));
    env = *backup;
    backup.reset(NULL);
    dungeon_events.clear();
    env.markers.activate_all(false);

    _corrupt_level_features(*abyssal);
    run_corruption_effects(300);

#ifndef USE_TILE
    // Allow extra time for the flash to linger.
    delay(1000);
#endif

    return (true);
}
