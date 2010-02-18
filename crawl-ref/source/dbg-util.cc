/*
 *  File:       dbg-util.cc
 *  Summary:    Miscellaneous debugging functions.
 *  Written by: Linley Henzell and Jesse Jones
 */

#include "AppHdr.h"

#include "dbg-util.h"

#include "artefact.h"
#include "cio.h"
#include "coord.h"
#include "dungeon.h"
#include "env.h"
#include "libutil.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "religion.h"
#include "shopping.h"
#include "skills2.h"
#include "spl-util.h"

//---------------------------------------------------------------
//
// debug_prompt_for_int
//
// If nonneg, then it returns a non-negative number or -1 on fail
// If !nonneg, then it returns an integer, and 0 on fail
//
//---------------------------------------------------------------
int debug_prompt_for_int( const char *prompt, bool nonneg )
{
    char specs[80];

    mpr(prompt, MSGCH_PROMPT);
    get_input_line( specs, sizeof( specs ) );

    if (specs[0] == '\0')
        return (nonneg ? -1 : 0);

    char *end;
    int   ret = strtol( specs, &end, 10 );

    if (ret < 0 && nonneg || ret == 0 && end == specs)
        ret = (nonneg ? -1 : 0);

    return (ret);
}

monster_type debug_prompt_for_monster(void)
{
    char  specs[80];

    mpr("Which monster by name? ", MSGCH_PROMPT);
    if (!cancelable_get_line_autohist(specs, sizeof specs))
    {
        if (specs[0] == '\0')
            return (MONS_NO_MONSTER);

        return (get_monster_by_name(specs));
    }
    return (MONS_NO_MONSTER);
}

void debug_dump_levgen()
{
    if (crawl_state.arena)
        return;

    CrawlHashTable &props = env.properties;

    std::string method;
    std::string type;

    if (Generating_Level)
    {
        mpr("Currently generating level.");
        extern std::string dgn_Build_Method;
        method = dgn_Build_Method;
        type   = dgn_Layout_Type;
    }
    else
    {
        if (!props.exists(BUILD_METHOD_KEY))
            method = "ABSENT";
        else
            method = props[BUILD_METHOD_KEY].get_string();

        if (!props.exists(LAYOUT_TYPE_KEY))
            type = "ABSENT";
        else
            type = props[LAYOUT_TYPE_KEY].get_string();
    }

    mprf("dgn_Build_Method = %s", method.c_str());
    mprf("dgn_Layout_Type  = %s", type.c_str());

    std::string extra;

    if (!props.exists(LEVEL_EXTRAS_KEY))
        extra = "ABSENT";
    else
    {
        const CrawlVector &vec = props[LEVEL_EXTRAS_KEY].get_vector();

        for (unsigned int i = 0; i < vec.size(); ++i)
            extra += vec[i].get_string() + ", ";
    }

    mprf("Level extras: %s", extra.c_str());

    mpr("Level vaults:");
    if (!props.exists(LEVEL_VAULTS_KEY))
        mpr("ABSENT");
    else
    {
        const CrawlHashTable &vaults = props[LEVEL_VAULTS_KEY].get_table();
        CrawlHashTable::const_iterator i = vaults.begin();

        for (; i != vaults.end(); ++i)
            mprf("    %s: %s", i->first.c_str(),
                 i->second.get_string().c_str());
    }
    mpr("");

    mpr("Temp vaults:");
    if (!props.exists(TEMP_VAULTS_KEY))
        mpr("ABSENT");
    else
    {
        const CrawlHashTable &vaults = props[TEMP_VAULTS_KEY].get_table();
        if (!vaults.empty())
        {
            CrawlHashTable::const_iterator i = vaults.begin();

            for (; i != vaults.end(); ++i)
            {
                mprf("    %s: %s", i->first.c_str(),
                     i->second.get_string().c_str());
            }
        }
    }
    mpr("");
}

void error_message_to_player(void)
{
    mpr("Oh dear. There appears to be a bug in the program.");
    mpr("I suggest you leave this level then save as soon as possible.");

}

std::string debug_coord_str(const coord_def &pos)
{
    return make_stringf("(%d, %d)%s", pos.x, pos.y,
                        !in_bounds(pos) ? " <OoB>" : "");
}

std::string debug_mon_str(const monsters* mon)
{
    const int midx = mon->mindex();
    if (invalid_monster_index(midx))
        return make_stringf("Invalid monster index %d", midx);

    std::string out = "Monster '" + mon->full_name(DESC_PLAIN, true) + "' ";
    out += make_stringf("%s [midx = %d]", debug_coord_str(mon->pos()).c_str(),
                        midx);

    return (out);
}

void debug_dump_mon(const monsters* mon, bool recurse)
{
    const int midx = mon->mindex();
    if (invalid_monster_index(midx) || invalid_monster_type(mon->type))
        return;

    fprintf(stderr, "<<<<<<<<<" EOL);

    fprintf(stderr, "Name: %s" EOL, mon->name(DESC_PLAIN, true).c_str());
    fprintf(stderr, "Base name: %s" EOL,
            mon->base_name(DESC_PLAIN, true).c_str());
    fprintf(stderr, "Full name: %s" EOL EOL,
            mon->full_name(DESC_PLAIN, true).c_str());

    if (in_bounds(mon->pos()))
    {
        std::string feat =
            raw_feature_description(grd(mon->pos()), NUM_TRAPS, true);
        fprintf(stderr, "On/in/over feature: %s" EOL EOL, feat.c_str());
    }

    fprintf(stderr, "Foe: ");
    if (mon->foe == MHITNOT)
        fprintf(stderr, "none");
    else if (mon->foe == MHITYOU)
        fprintf(stderr, "player");
    else if (invalid_monster_index(mon->foe))
        fprintf(stderr, "invalid monster index %d", mon->foe);
    else if (mon->foe == midx)
        fprintf(stderr, "self");
    else
        fprintf(stderr, "%s", debug_mon_str(&menv[mon->foe]).c_str());

    fprintf(stderr, EOL);

    fprintf(stderr, "Target: ");
    if (mon->target.origin())
        fprintf(stderr, "none" EOL);
    else
        fprintf(stderr, "%s" EOL, debug_coord_str(mon->target).c_str());

    int target = MHITNOT;
    fprintf(stderr, "At target: ");
    if (mon->target.origin())
        fprintf(stderr, "N/A");
    else if (mon->target == you.pos())
    {
        fprintf(stderr, "player");
        target = MHITYOU;
    }
    else if (mon->target == mon->pos())
    {
        fprintf(stderr, "self");
        target = midx;
    }
    else if (in_bounds(mon->target))
    {
       target = mgrd(mon->target);

       if (target == NON_MONSTER)
           fprintf(stderr, "nothing");
       else if (target == midx)
           fprintf(stderr, "improperly linked self");
       else if (target == mon->foe)
           fprintf(stderr, "same as foe");
       else if (invalid_monster_index(target))
           fprintf(stderr, "invalid monster index %d", target);
       else
           fprintf(stderr, "%s", debug_mon_str(&menv[target]).c_str());
    }
    else
        fprintf(stderr, "<OoB>");

    fprintf(stderr, EOL);

    if (mon->is_patrolling())
    {
        fprintf(stderr, "Patrolling: %s" EOL EOL,
                debug_coord_str(mon->patrol_point).c_str());
    }

    if (mon->travel_target != MTRAV_NONE)
    {
        fprintf(stderr, EOL "Travelling:" EOL);
        fprintf(stderr, "    travel_target      = %d" EOL, mon->travel_target);
        fprintf(stderr, "    travel_path.size() = %lu" EOL,
                (unsigned long) mon->travel_path.size());

        if (mon->travel_path.size() > 0)
        {
            fprintf(stderr, "    next travel step: %s" EOL,
                    debug_coord_str(mon->travel_path.back()).c_str());
            fprintf(stderr, "    last travel step: %s" EOL,
                    debug_coord_str(mon->travel_path.front()).c_str());
        }
    }
    fprintf(stderr, EOL);

    fprintf(stderr, "Inventory:" EOL);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
    {
        const int idx = mon->inv[i];

        if (idx == NON_ITEM)
            continue;

        fprintf(stderr, "    slot #%d: ", i);

        if (idx < 0 || idx > MAX_ITEMS)
        {
            fprintf(stderr, "invalid item index %d" EOL, idx);
            continue;
        }
        const item_def &item(mitm[idx]);

        if (!item.is_valid())
        {
            fprintf(stderr, "invalid item" EOL);
            continue;
        }

        fprintf(stderr, "%s", item.name(DESC_PLAIN, false, true).c_str());

        if (!item.held_by_monster())
        {
            fprintf(stderr, " [not held by monster, pos = %s]",
                    debug_coord_str(item.pos).c_str());
        }
        else if (item.holding_monster() != mon)
        {
            fprintf(stderr, " [held by other monster: %s]",
                    debug_mon_str(item.holding_monster()).c_str());
        }

        fprintf(stderr, EOL);
    }
    fprintf(stderr, EOL);

    if (mon->can_use_spells())
    {
        fprintf(stderr, "Spells:" EOL);

        for (int i = 0; i < NUM_MONSTER_SPELL_SLOTS; ++i)
        {
            spell_type spell = mon->spells[i];

            if (spell == SPELL_NO_SPELL)
                continue;

            fprintf(stderr, "    slot #%d: ", i);
            if (!is_valid_spell(spell))
                fprintf(stderr, "Invalid spell #%d" EOL, (int) spell);
            else
                fprintf(stderr, "%s" EOL, spell_title(spell));
        }
        fprintf(stderr, EOL);
    }

    fprintf(stderr, "attitude: %d, behaviour: %d, number: %d, flags: 0x%lx" EOL,
            mon->attitude, mon->behaviour, mon->number, mon->flags);

    fprintf(stderr, "colour: %d, foe_memory: %d, shield_blocks:%d, "
                  "experience: %lu" EOL,
            mon->colour, mon->foe_memory, mon->shield_blocks,
            mon->experience);

    fprintf(stderr, "god: %s, seen_context: %s" EOL,
            god_name(mon->god).c_str(), mon->seen_context.c_str());

    fprintf(stderr, ">>>>>>>>>" EOL EOL);

    if (!recurse)
        return;

    if (!invalid_monster_index(mon->foe) && mon->foe != midx
        && !invalid_monster_type(menv[mon->foe].type))
    {
        fprintf(stderr, "Foe:" EOL);
        debug_dump_mon(&menv[mon->foe], false);
    }

    if (!invalid_monster_index(target) && target != midx
        && target != mon->foe
        && !invalid_monster_type(menv[target].type))
    {
        fprintf(stderr, "Target:" EOL);
        debug_dump_mon(&menv[target], false);
    }
}

//---------------------------------------------------------------
//
// debug_prompt_for_skill
//
//---------------------------------------------------------------
int debug_prompt_for_skill( const char *prompt )
{
    char specs[80];

    mpr(prompt, MSGCH_PROMPT);
    get_input_line( specs, sizeof( specs ) );

    if (specs[0] == '\0')
        return (-1);

    int skill = -1;

    for (int i = 0; i < NUM_SKILLS; ++i)
    {
        // Avoid the bad values.
        if (is_invalid_skill(i))
            continue;

        char sk_name[80];
        strncpy( sk_name, skill_name(i), sizeof( sk_name ) );

        char *ptr = strstr( strlwr(sk_name), strlwr(specs) );
        if (ptr != NULL)
        {
            if (ptr == sk_name && strlen(specs) > 0)
            {
                // We prefer prefixes over partial matches.
                skill = i;
                break;
            }
            else
                skill = i;
        }
    }

    return (skill);
}

std::string debug_art_val_str(const item_def& item)
{
    ASSERT(is_artefact(item));

    return make_stringf("art val: %d, item val: %d",
                        artefact_value(item), item_value(item, true));
}

int debug_cap_stat(int stat)
{
    return (stat <  1  ?   1 :
            stat > 127 ? 127
                       : stat);
}
