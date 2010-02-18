/*  File:       mislead.cc
 *  Summary:    Handling of Mara's Mislead spell and stats, plus fakes.
 */

#include "AppHdr.h"
#include "mislead.h"

#include "enum.h"
#include "env.h"
#include "message.h"
#include "monster.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "view.h"
#include "random.h"
#include "tutorial.h"
#include "xom.h"

bool unsuitable_misled_monster(monster_type mons)
{
    return (mons_is_unique(mons) || mons_is_mimic(mons)
            || mons_class_is_stationary(mons)
            || mons_genus(mons) == MONS_DRACONIAN
            || mons == MONS_DANCING_WEAPON || mons == MONS_UGLY_THING
            || mons == MONS_VERY_UGLY_THING || mons == MONS_ZOMBIE_SMALL
            || mons == MONS_ZOMBIE_LARGE || mons == MONS_SKELETON_SMALL
            || mons == MONS_SKELETON_LARGE || mons == MONS_SIMULACRUM_SMALL
            || mons == MONS_SIMULACRUM_LARGE || mons == MONS_SPECTRAL_THING
            || mons == MONS_SLIME_CREATURE || mons == MONS_BALLISTOMYCETE
            || mons == MONS_HYDRA
            || mons_is_pghost(mons)
            || mons == MONS_SHAPESHIFTER || mons == MONS_PANDEMONIUM_DEMON
            || mons == MONS_KILLER_KLOWN || mons == MONS_KRAKEN
            || mons == MONS_KRAKEN_TENTACLE
            || mons == MONS_GLOWING_SHAPESHIFTER
            || mons == MONS_GIANT_BAT);
}

monster_type get_misled_monster (monsters *monster)
{
    monster_type mons = random_monster_at_grid(monster->pos());
    if (unsuitable_misled_monster(mons))
        mons = random_monster_at_grid(monster->pos());

    if (unsuitable_misled_monster(mons))
        return (MONS_GIANT_BAT);

    return mons;
}

bool update_mislead_monster(monsters* monster)
{
    // Don't affect uniques, named monsters, and monsters with special tiles.
    if (mons_is_unique(monster->type) || !monster->mname.empty()
        || monster->props.exists("monster_tile")
        || monster->props.exists("mislead_as"))
    {
        return (false);
    }

    short misled_as = get_misled_monster(monster);
    monster->props["mislead_as"] = misled_as;

    if (misled_as == MONS_GIANT_BAT)
        return (false);

    return (true);
}

int update_mislead_monsters(monsters* caster)
{
    int count = 0;

    for (monster_iterator mi; mi; ++mi)
        if (*mi != caster && update_mislead_monster(*mi))
            count++;

    return count;
}

void mons_cast_mislead(monsters *monster)
{
    // This really only affects the player; it causes confusion when cast on
    // non-player foes, but that is dealt with inside ye-great-Switch-of-Doom.
    if (monster->foe != MHITYOU)
        return;

    // We deal with pointless misleads in the right place now.

    if (wearing_amulet(AMU_CLARITY))
    {
        mpr("Your vision blurs momentarily.");
        return;
    }

    update_mislead_monsters(monster);

    const int old_value = you.duration[DUR_MISLED];
    you.increase_duration(DUR_MISLED, monster->hit_dice * 12 / 3, 50);
    if (you.duration[DUR_MISLED] > old_value)
    {
        you.check_awaken(500);

        if (old_value <= 0)
        {
            mpr("But for a moment, strange images dance in front of your eyes.", MSGCH_WARN);
#ifdef USE_TILE
            tiles.add_overlay(you.pos(), tileidx_zap(MAGENTA));
            update_screen();
#else
            flash_view(MAGENTA);
#endif
            more();
        }
        else
            mpr("You are even more misled!", MSGCH_WARN);

        learned_something_new(TUT_YOU_ENCHANTED);

        xom_is_stimulated((you.duration[DUR_MISLED] - old_value)
                           / BASELINE_DELAY);
    }

    return;
}

int count_mara_fakes()
{
    int count = 0;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->type == MONS_MARA_FAKE)
            count++;
    }

    return count;
}
