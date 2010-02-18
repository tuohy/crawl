/*
 * File:     mon-info.cc
 * Summary:  Monster information that may be passed to the user.
 *
 * Used to fill the monster pane and to pass monster info to Lua.
 */

#include "AppHdr.h"

#include "mon-info.h"

#include "fight.h"
#include "libutil.h"
#include "misc.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "monster.h"
#include "options.h"
#include "religion.h"
#include "showsymb.h"
#include "state.h"

#include <algorithm>
#include <sstream>

enum monster_info_brands
{
    MB_STABBABLE,
    MB_DISTRACTED,
    MB_BERSERK
};

monster_info::monster_info(const monsters *m)
    : m_mon(m), m_attitude(ATT_HOSTILE), m_difficulty(0),
      m_brands(0), m_fullname(true)
{
    // XXX: this doesn't take into account ENCH_TEMP_PACIF, but that's probably
    // a bug for mons_attitude, not this.
    // XXX: also, mons_attitude_type should be sorted hostile/neutral/friendly;
    // will break saves a little bit though.
    m_attitude = mons_attitude(m);

    int mtype = m->type;
    if (mtype == MONS_RAKSHASA_FAKE)
        mtype = MONS_RAKSHASA;
    else if (mtype == MONS_MARA_FAKE)
        mtype = MONS_MARA;

    // Currently, difficulty is defined as "average hp".
    m_difficulty = mons_difficulty(mtype);

    if (mons_looks_stabbable(m))   m_brands |= (1 << MB_STABBABLE);
    if (mons_looks_distracted(m))  m_brands |= (1 << MB_DISTRACTED);
    if (m->berserk())              m_brands |= (1 << MB_BERSERK);

    glyph g = get_mons_glyph(m_mon);
    m_glyph = g.ch;
    m_glyph_colour = g.col;

    mons_get_damage_level(m_mon, m_damage_desc, m_damage_level);
    // If no messages about wounds, don't display damage level either.
    if (monster_descriptor(m_mon->type, MDSC_NOMSG_WOUNDS))
        m_damage_level = MDAM_OKAY;
}

// Needed because gcc 4.3 sort does not like comparison functions that take
// more than 2 arguments.
bool monster_info::less_than_wrapper(const monster_info& m1,
                                     const monster_info& m2)
{
    return monster_info::less_than(m1, m2, true);
}

// Sort monsters by (in that order):    attitude, difficulty, type, brand
bool monster_info::less_than(const monster_info& m1,
                                  const monster_info& m2, bool zombified)
{
    if (m1.m_attitude < m2.m_attitude)
        return (true);
    else if (m1.m_attitude > m2.m_attitude)
        return (false);

    int m1type = m1.m_mon->type;
    int m2type = m2.m_mon->type;
    if (!crawl_state.arena && you.misled())
    {
        m1type = m1.m_mon->get_mislead_type();
        m2type = m2.m_mon->get_mislead_type();
    }

    // Don't differentiate real rakshasas from fake ones.
    if (m1type == MONS_RAKSHASA_FAKE)
        m1type = MONS_RAKSHASA;
    else if (m1type == MONS_MARA_FAKE)
        m1type = MONS_MARA;
    if (m2type == MONS_RAKSHASA_FAKE)
        m2type = MONS_RAKSHASA;
    else if (m2type == MONS_MARA_FAKE)
        m2type = MONS_MARA;

    // Force plain but different coloured draconians to be treated like the
    // same sub-type.
    if (!zombified && m1type >= MONS_DRACONIAN
        && m1type <= MONS_PALE_DRACONIAN
        && m2type >= MONS_DRACONIAN
        && m2type <= MONS_PALE_DRACONIAN)
    {
        return (false);
    }

    // By descending difficulty
    if (m1.m_difficulty > m2.m_difficulty)
        return (true);
    else if (m1.m_difficulty < m2.m_difficulty)
        return (false);

    // Force mimics of different types to be treated like the same one.
    if (mons_is_mimic(m1type) && mons_is_mimic(m2type))
        return (false);

    if (m1type < m2type)
        return (true);
    else if (m1type > m2type)
        return (false);

    // Never distinguish between dancing weapons.
    // The above checks guarantee that *both* monsters are of this type.
    if (m1type == MONS_DANCING_WEAPON)
        return (false);

    if (m1type == MONS_SLIME_CREATURE)
        return (m1.m_mon->number > m2.m_mon->number);

    if (m1type == MONS_BALLISTOMYCETE)
        return ((m1.m_mon->number > 0) > (m2.m_mon->number > 0));

    if (zombified)
    {
        // Because of the type checks above, if one of the two is zombified, so
        // is the other, and of the same type.
        if (mons_is_zombified(m1.m_mon)
            && m1.m_mon->base_monster < m2.m_mon->base_monster)
        {
            return (true);
        }

        // Both monsters are hydras or hydra zombies, sort by number of heads.
        if (m1.m_mon->has_hydra_multi_attack()
            && m1.m_mon->number > m2.m_mon->number)
        {
            return (true);
        }
    }

    if (m1.m_fullname && m2.m_fullname || mons_is_pghost(m1type))
        return (m1.m_mon->name(DESC_PLAIN) < m2.m_mon->name(DESC_PLAIN));

#if 0 // for now, sort brands together.
    // By descending brands, so no brands sorts to the end
    if (m1.m_brands > m2.m_brands)
        return (true);
    else if (m1.m_brands < m2.m_brands)
        return (false);
#endif

    return (false);
}

static std::string _verbose_info(const monsters* m)
{
    if (m->caught())
        return (" (caught)");

    if (mons_behaviour_perceptible(m))
    {
        if (m->petrified())
            return (" (petrified)");
        if (m->paralysed())
            return (" (paralysed)");
        if (m->petrifying())
            return (" (petrifying)");
        if (mons_is_confused(m))
            return (" (confused)");
        if (mons_is_fleeing(m))
            return (" (fleeing)");
        if (m->asleep())
        {
            if (!m->can_hibernate(true))
                return (" (dormant)");
            else
                return (" (sleeping)");
        }
        if (mons_is_wandering(m) && !mons_is_batty(m)
            && !(m->attitude == ATT_STRICT_NEUTRAL))
        {
            // Labeling strictly neutral monsters as fellow slimes is more important.
            return (" (wandering)");
        }
        if (m->foe == MHITNOT && !mons_is_batty(m) && !m->neutral()
            && !m->friendly())
        {
            return (" (unaware)");
        }
    }

    if (m->has_ench(ENCH_STICKY_FLAME))
        return (" (burning)");

    if (m->has_ench(ENCH_ROT))
        return (" (rotting)");

    if (m->has_ench(ENCH_INVIS))
        return (" (invisible)");

    return ("");
}

void monster_info::to_string(int count, std::string& desc,
                                  int& desc_color) const
{
    std::ostringstream out;
    monster_type type = m_mon->type;
    if (!crawl_state.arena && you.misled())
        type = m_mon->get_mislead_type();

    if (count == 1)
    {
        if (mons_is_mimic(type))
            out << mons_type_name(type, DESC_PLAIN);
        else
            out << m_mon->full_name(DESC_PLAIN);
    }
    else
    {
        // Don't pluralise uniques, ever.  Multiple copies of the same unique
        // are unlikely in the dungeon currently, but quite common in the
        // arena.  This prevens "4 Gra", etc. {due}
        // Unless it's Mara, who summons illusions of himself.
        if (mons_is_unique(type) && type != MONS_MARA
            && type != MONS_MARA_FAKE)
        {
            out << count << " "
                << m_mon->name(DESC_PLAIN);
        }
        // Specialcase mimics, so they don't get described as piles of gold
        // when that would be inappropriate. (HACK)
        else if (mons_is_mimic(type))
        {
            out << count << " mimics";
        }
        // Don't differentiate between dancing weapons, (very)
        // ugly things or draconians of different types.
        else if (m_fullname
                 && type != MONS_DANCING_WEAPON
                 && mons_genus(type) != MONS_DRACONIAN
                 && type != MONS_UGLY_THING
                 && type != MONS_VERY_UGLY_THING
                 && m_mon->mname.empty())
        {
            out << count << " "
                << pluralise(m_mon->name(DESC_PLAIN));
        }
        else if (type >= MONS_DRACONIAN
                 && type <= MONS_PALE_DRACONIAN)
        {
            out << count << " "
                << pluralise(mons_type_name(MONS_DRACONIAN, DESC_PLAIN));
        }
        else
        {
            out << count << " "
                << pluralise(mons_type_name(type, DESC_PLAIN));
        }
    }

#if DEBUG_DIAGNOSTICS
    out << " av" << m_difficulty << " "
        << m_mon->hit_points << "/" << m_mon->max_hit_points;
#endif

    if (count == 1)
    {
        if (m_mon->frenzied())
            out << " (frenzied)";
        else if (m_mon->berserk())
            out << " (berserk)";
        else if (Options.verbose_monster_pane)
            out << _verbose_info(m_mon);
        else if (mons_looks_stabbable(m_mon))
            out << " (resting)";
        else if (mons_looks_distracted(m_mon))
            out << " (distracted)";
        else if (m_mon->has_ench(ENCH_INVIS))
            out << " (invisible)";
    }

    // Friendliness
    switch (m_attitude)
    {
    case ATT_FRIENDLY:
        //out << " (friendly)";
        desc_color = GREEN;
        break;
    case ATT_GOOD_NEUTRAL:
    case ATT_NEUTRAL:
        //out << " (neutral)";
        desc_color = BROWN;
        break;
    case ATT_STRICT_NEUTRAL:
         out << " (fellow slime)";
         desc_color = BROWN;
         break;
    case ATT_HOSTILE:
        // out << " (hostile)";
        desc_color = LIGHTGREY;
        break;
    }

    // Evilness of attacking
    switch (m_attitude)
    {
    case ATT_NEUTRAL:
    case ATT_HOSTILE:
        if (count == 1 && you.religion == GOD_SHINING_ONE
            && !tso_unchivalric_attack_safe_monster(m_mon)
            && is_unchivalric_attack(&you, m_mon))
        {
            desc_color = Options.evil_colour;
        }
        break;
    default:
        break;
    }

    desc = out.str();
}

void get_monster_info(std::vector<monster_info>& mons)
{
    std::vector<monsters*> visible;
    if (crawl_state.arena)
    {
        for (monster_iterator mi; mi; ++mi)
            visible.push_back(*mi);
    }
    else
        visible = get_nearby_monsters();

    for (unsigned int i = 0; i < visible.size(); i++)
    {
        if (!mons_class_flag( visible[i]->type, M_NO_EXP_GAIN )
            || visible[i]->type == MONS_KRAKEN_TENTACLE
            || visible[i]->type == MONS_BALLISTOMYCETE
                && visible[i]->number > 0)
        {
            mons.push_back(monster_info(visible[i]));
        }
    }
    std::sort(mons.begin(), mons.end(), monster_info::less_than_wrapper);
}
