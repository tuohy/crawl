#include "AppHdr.h"

#include "godprayer.h"

#include <cmath>

#include "areas.h"
#include "artefact.h"
#include "coordit.h"
#include "effects.h"
#include "env.h"
#include "food.h"
#include "fprop.h"
#include "godabil.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "player.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "monster.h"
#include "notes.h"
#include "options.h"
#include "random.h"
#include "religion.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "view.h"

static bool _confirm_pray_sacrifice(god_type god)
{
    if (Options.stash_tracking == STM_EXPLICIT && is_stash(you.pos()))
    {
        mpr("You can't sacrifice explicitly marked stashes.");
        return (false);
    }

    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (god_likes_item(god, *si)
            && has_warning_inscription(*si, OPER_PRAY))
        {
            std::string prompt = "Really sacrifice stack with ";
            prompt += si->name(DESC_NOCAP_A);
            prompt += " in it?";

            if (!yesno(prompt.c_str(), false, 'n'))
                return (false);
        }
    }

    return (true);
}

std::string god_prayer_reaction()
{
    std::string result = god_name(you.religion);
    if (crawl_state.player_is_dead())
        result += " was ";
    else
        result += " is ";
    result +=
        (you.piety > 130) ? "exalted by your worship" :
        (you.piety > 100) ? "extremely pleased with you" :
        (you.piety >  70) ? "greatly pleased with you" :
        (you.piety >  40) ? "most pleased with you" :
        (you.piety >  20) ? "pleased with you" :
        (you.piety >   5) ? "noncommittal"
                          : "displeased";
    result += ".";

    return (result);
}

bool god_accepts_prayer(god_type god)
{
    harm_protection_type hpt = god_protects_from_harm(god, false);

    if (hpt == HPT_PRAYING || hpt == HPT_PRAYING_PLUS_ANYTIME
        || hpt == HPT_RELIABLE_PRAYING_PLUS_ANYTIME)
    {
        return (true);
    }

    if (god_likes_fresh_corpses(god))
        return (true);

    switch (god)
    {
    case GOD_ZIN:
        return (zin_sustenance(false));

    case GOD_KIKUBAAQUDGHA:
        return (you.piety >= piety_breakpoint(4));

    case GOD_YREDELEMNUL:
        return (yred_injury_mirror(false));

    case GOD_JIYVA:
        return (jiyva_grant_jelly(false));

    case GOD_BEOGH:
    case GOD_NEMELEX_XOBEH:
        return (true);

    default:
        break;
    }

    return (false);
}

static bool _bless_weapon(god_type god, brand_type brand, int colour)
{
    item_def& wpn = *you.weapon();

    if (is_artefact(wpn) || (is_range_weapon(wpn) && brand != SPWPN_HOLY_WRATH))
        return (false);

    std::string prompt = "Do you wish to have your weapon ";
    if (brand == SPWPN_PAIN)
        prompt += "bloodied with pain";
    else if (brand == SPWPN_DISTORTION)
        prompt += "corrupted";
    else
        prompt += "blessed";
    prompt += "?";

    if (!yesno(prompt.c_str(), true, 'n'))
        return (false);

    you.duration[DUR_WEAPON_BRAND] = 0;     // just in case

    std::string old_name = wpn.name(DESC_NOCAP_A);
    set_equip_desc(wpn, ISFLAG_GLOWING);
    set_item_ego_type(wpn, OBJ_WEAPONS, brand);
    wpn.colour = colour;

    const bool is_cursed = wpn.cursed();

    enchant_weapon(ENCHANT_TO_HIT, true, wpn);

    if (coinflip())
        enchant_weapon(ENCHANT_TO_HIT, true, wpn);

    enchant_weapon(ENCHANT_TO_DAM, true, wpn);

    if (coinflip())
        enchant_weapon(ENCHANT_TO_DAM, true, wpn);

    if (is_cursed)
        do_uncurse_item(wpn);

    if (god == GOD_SHINING_ONE)
    {
        convert2good(wpn);

        if (is_blessed_convertible(wpn))
        {
            origin_acquired(wpn, GOD_SHINING_ONE);
            wpn.flags |= ISFLAG_BLESSED_WEAPON;
        }

        burden_change();
    }
    else if (is_evil_god(god))
    {
        convert2bad(wpn);

        burden_change();
    }

    you.wield_change = true;
    you.num_gifts[god]++;
    std::string desc  = old_name + " ";
            desc += (god == GOD_SHINING_ONE   ? "blessed by the Shining One" :
                     god == GOD_LUGONU        ? "corrupted by Lugonu" :
                     god == GOD_KIKUBAAQUDGHA ? "bloodied by Kikubaaqudgha"
                                              : "touched by the gods");

    take_note(Note(NOTE_ID_ITEM, 0, 0,
              wpn.name(DESC_NOCAP_A).c_str(), desc.c_str()));
    wpn.flags |= ISFLAG_NOTED_ID;

    mpr("Your weapon shines brightly!", MSGCH_GOD);

    flash_view(colour);

    simple_god_message(" booms: Use this gift wisely!");

    if (god == GOD_SHINING_ONE)
    {
        holy_word(100, HOLY_WORD_TSO, you.pos(), true);

        // Un-bloodify surrounding squares.
        for (radius_iterator ri(you.pos(), 3, true, true); ri; ++ri)
            if (is_bloodcovered(*ri))
                env.pgrid(*ri) &= ~(FPROP_BLOODY);
    }

    if (god == GOD_KIKUBAAQUDGHA)
    {
        torment(TORMENT_KIKUBAAQUDGHA, you.pos());

        // Bloodify surrounding squares (75% chance).
        for (radius_iterator ri(you.pos(), 2, true, true); ri; ++ri)
            if (!one_chance_in(4))
                maybe_bloodify_square(*ri);
    }

#ifndef USE_TILE
    // Allow extra time for the flash to linger.
    delay(1000);
#endif

    return (true);
}

static bool _altar_prayer()
{
    // Different message from when first joining a religion.
    mpr("You prostrate yourself in front of the altar and pray.");

    if (you.religion == GOD_XOM)
        return (false);

    god_acting gdact;

    bool did_bless = false;

    // TSO blesses weapons with holy wrath, and long blades and demon
    // whips specially.
    if (you.religion == GOD_SHINING_ONE
        && !you.num_gifts[GOD_SHINING_ONE]
        && !player_under_penance()
        && you.piety > 160)
    {
        item_def *wpn = you.weapon();

        if (wpn
            && (get_weapon_brand(*wpn) != SPWPN_HOLY_WRATH
                || is_blessed_convertible(*wpn)))
        {
            did_bless = _bless_weapon(GOD_SHINING_ONE, SPWPN_HOLY_WRATH,
                                      YELLOW);
        }
    }

    // Lugonu blesses weapons with distortion.
    if (you.religion == GOD_LUGONU
        && !you.num_gifts[GOD_LUGONU]
        && !player_under_penance()
        && you.piety > 160)
    {
        item_def *wpn = you.weapon();

        if (wpn && get_weapon_brand(*wpn) != SPWPN_DISTORTION)
            did_bless = _bless_weapon(GOD_LUGONU, SPWPN_DISTORTION, MAGENTA);
    }

    // Kikubaaqudgha blesses weapons with pain, or gives you a Necronomicon.
    if (you.religion == GOD_KIKUBAAQUDGHA
        && !you.num_gifts[GOD_KIKUBAAQUDGHA]
        && !player_under_penance()
        && you.piety > 160)
    {
        simple_god_message(
            " will bloody your weapon with pain or grant you the Necronomicon.");

        bool kiku_did_bless_weapon = false;

        item_def *wpn = you.weapon();

        // Does the player want a pain branding?
        if (wpn && get_weapon_brand(*wpn) != SPWPN_PAIN)
        {
            kiku_did_bless_weapon =
                _bless_weapon(GOD_KIKUBAAQUDGHA, SPWPN_PAIN, RED);
            did_bless = kiku_did_bless_weapon;
        }
        else
            mpr("You have no weapon to bloody with pain.");

        // If not, ask if the player wants a Necronomicon.
        if (!kiku_did_bless_weapon)
        {
            if (!yesno("Do you wish to receive the Necronomicon?", true, 'n'))
                return (false);

            int thing_created = items(1, OBJ_BOOKS, BOOK_NECRONOMICON, true, 1,
                                      MAKE_ITEM_RANDOM_RACE,
                                      0, 0, you.religion);

            if (thing_created == NON_ITEM)
                return (false);

            move_item_to_grid(&thing_created, you.pos());

            if (thing_created != NON_ITEM)
            {
                simple_god_message(" grants you a gift!");
                more();

                you.num_gifts[you.religion]++;
                did_bless = true;
                take_note(Note(NOTE_GOD_GIFT, you.religion));
                mitm[thing_created].inscription = "god gift";
            }
        }

        // Return early so we don't offer our Necronomicon to Kiku.
        return (did_bless);
    }

    offer_items();

    return (did_bless);
}

void pray()
{
    if (silenced(you.pos()))
    {
        mpr("You are unable to make a sound!");
        return;
    }

    // almost all prayers take time
    you.turn_is_over = true;

    const bool was_praying = !!you.duration[DUR_PRAYER];

    bool something_happened = false;
    const god_type altar_god = feat_altar_god(grd(you.pos()));
    if (altar_god != GOD_NO_GOD)
    {
        if (you.flight_mode() == FL_LEVITATE)
        {
            you.turn_is_over = false;
            mpr("You are floating high above the altar.");
            return;
        }

        if (you.religion != GOD_NO_GOD && altar_god == you.religion)
        {
            something_happened = _altar_prayer();
        }
        else if (altar_god != GOD_NO_GOD)
        {
            if (you.species == SP_DEMIGOD)
            {
                you.turn_is_over = false;
                mpr("Sorry, a being of your status cannot worship here.");
                return;
            }

            god_pitch(feat_altar_god(grd(you.pos())));
            return;
        }
    }

    if (you.religion == GOD_NO_GOD)
    {
        mprf(MSGCH_PRAY,
             "You spend a moment contemplating the meaning of %slife.",
             you.is_undead ? "un" : "");

        // Zen meditation is timeless.
        you.turn_is_over = false;
        return;
    }
    else if (!god_accepts_prayer(you.religion))
    {
        if (!something_happened)
        {
            simple_god_message(" ignores your prayer.");
            you.turn_is_over = false;
        }
        return;
    }

    // Beoghites and Nemelexites can abort now instead of offering
    // something they don't want to lose.
    if (altar_god == GOD_NO_GOD
        && (you.religion == GOD_BEOGH ||  you.religion == GOD_NEMELEX_XOBEH)
        && !_confirm_pray_sacrifice(you.religion))
    {
        you.turn_is_over = false;
        return;
    }

    mprf(MSGCH_PRAY, "You %s prayer to %s.",
         was_praying ? "renew your" : "offer a",
         god_name(you.religion).c_str());

    you.duration[DUR_PRAYER] = 9 + (random2(you.piety) / 20)
                                 + (random2(you.piety) / 20);

    if (player_under_penance())
        simple_god_message(" demands penance!");
    else
    {
        mpr(god_prayer_reaction().c_str(), MSGCH_PRAY, you.religion);

        if (you.piety > 130)
            you.duration[DUR_PRAYER] *= 3;
        else if (you.piety > 70)
            you.duration[DUR_PRAYER] *= 2;
    }

    // Assume for now that gods who like fresh corpses and/or butchery
    // don't use prayer for anything else.
    if (you.religion == GOD_ZIN
        || you.religion == GOD_KIKUBAAQUDGHA
        || you.religion == GOD_BEOGH
        || you.religion == GOD_NEMELEX_XOBEH
        || you.religion == GOD_JIYVA
        || god_likes_fresh_corpses(you.religion))
    {
        you.duration[DUR_PRAYER] = 1;
    }
    else if (you.religion == GOD_ELYVILON
        || you.religion == GOD_YREDELEMNUL)
    {
        you.duration[DUR_PRAYER] = 20;
    }

    // Gods who like fresh corpses, Kikuites, Beoghites and Nemelexites
    // offer the items they're standing on.
    if (altar_god == GOD_NO_GOD
        && (god_likes_fresh_corpses(you.religion)
            || you.religion == GOD_BEOGH || you.religion == GOD_NEMELEX_XOBEH))
    {
        offer_items();
    }

    if (!was_praying)
        do_god_gift(true);

    dprf("piety: %d (-%d)", you.piety, you.piety_hysteresis );
}

void end_prayer(void)
{
    mpr("Your prayer is over.", MSGCH_PRAY, you.religion);
    you.duration[DUR_PRAYER] = 0;
}

static int _leading_sacrifice_group()
{
    int weights[5];
    get_pure_deck_weights(weights);
    int best_i = -1, maxweight = -1;
    for ( int i = 0; i < 5; ++i )
    {
        if ( best_i == -1 || weights[i] > maxweight )
        {
            maxweight = weights[i];
            best_i = i;
        }
    }
    return best_i;
}

static void _give_sac_group_feedback(int which)
{
    ASSERT( which >= 0 && which < 5 );
    const char* names[] = {
        "Escape", "Destruction", "Dungeons", "Summoning", "Wonder"
    };
    mprf(MSGCH_GOD, "A symbol of %s coalesces before you, then vanishes.",
         names[which]);
}

// God effects of sacrificing one item from a stack (e.g., a weapon, one
// out of 20 arrows, etc.).  Does not modify the actual item in any way.
static piety_gain_t _sacrifice_one_item_noncount(const item_def& item)
{
    piety_gain_t relative_piety_gain = PIETY_NONE;

    if (god_likes_fresh_corpses(you.religion))
    {
        if (x_chance_in_y(13, 19))
        {
            gain_piety(1);
            relative_piety_gain = PIETY_SOME;
        }
    }
    else
    {
        switch (you.religion)
        {
        case GOD_SHINING_ONE:
            gain_piety(1);
            relative_piety_gain = PIETY_SOME;
            break;

        case GOD_BEOGH:
        {
            const int item_orig = item.orig_monnum - 1;

            int chance = 4;

            if (item_orig == MONS_SAINT_ROKA)
                chance += 12;
            else if (item_orig == MONS_ORC_HIGH_PRIEST)
                chance += 8;
            else if (item_orig == MONS_ORC_PRIEST)
                chance += 4;

            if (food_is_rotten(item))
                chance--;
            else if (item.sub_type == CORPSE_SKELETON)
                chance -= 2;

            if (x_chance_in_y(chance, 20))
            {
                gain_piety(1);
                relative_piety_gain = PIETY_SOME;
            }
            break;
        }

        case GOD_NEMELEX_XOBEH:
        {
            // item_value() multiplies by quantity.
            const int value = item_value(item) / item.quantity;

            if (you.attribute[ATTR_CARD_COUNTDOWN] && x_chance_in_y(value, 800))
            {
                you.attribute[ATTR_CARD_COUNTDOWN]--;
#if DEBUG_DIAGNOSTICS || DEBUG_CARDS || DEBUG_SACRIFICE
                mprf(MSGCH_DIAGNOSTICS, "Countdown down to %d",
                     you.attribute[ATTR_CARD_COUNTDOWN]);
#endif
            }
            // Nemelex piety gain is fairly fast... at least when you
            // have low piety.
            if (item.base_type == OBJ_CORPSES && one_chance_in(2 + you.piety/50)
                || x_chance_in_y(value/2 + 1, 30 + you.piety/2))
            {
                if (is_artefact(item))
                {
                    gain_piety(2);
                    relative_piety_gain = PIETY_LOTS;
                }
                else
                {
                    gain_piety(1);
                    relative_piety_gain = PIETY_SOME;
                }
            }

            if (item.base_type == OBJ_FOOD && item.sub_type == FOOD_CHUNK
                || is_blood_potion(item))
            {
                // Count chunks and blood potions towards decks of
                // Summoning.
                you.sacrifice_value[OBJ_CORPSES] += value;
            }
            else if (item.base_type == OBJ_CORPSES)
            {
#if DEBUG_GIFTS || DEBUG_CARDS || DEBUG_SACRIFICE
                mprf(MSGCH_DIAGNOSTICS, "Corpse mass is %d",
                     item_mass(item));
#endif
                you.sacrifice_value[item.base_type] += item_mass(item);
            }
            else
                you.sacrifice_value[item.base_type] += value;
            break;
        }

        default:
            break;
        }
    }

    return (relative_piety_gain);
}

static int _gold_to_donation(int gold)
{
    return static_cast<int>((gold * log((float)gold)) / MAX_PIETY);
}

void offer_items()
{
    if (you.religion == GOD_NO_GOD)
        return;

    int i = you.visible_igrd(you.pos());

    if (!god_likes_items(you.religion) && i != NON_ITEM)
    {
        simple_god_message(" doesn't care about such mundane gifts.",
                           you.religion);
        return;
    }

    god_acting gdact;

    // donate gold to gain piety distributed over time
    if (you.religion == GOD_ZIN)
    {
        if (you.gold == 0)
        {
            mpr("You don't have anything to sacrifice.");
            return;
        }

        if (!yesno("Do you wish to donate half of your money?", true, 'n'))
            return;

        const int donation_cost = (you.gold / 2) + 1;
        const int donation = _gold_to_donation(donation_cost);

#if DEBUG_DIAGNOSTICS || DEBUG_SACRIFICE || DEBUG_PIETY
        mprf(MSGCH_DIAGNOSTICS, "A donation of $%d amounts to an "
             "increase of piety by %d.", donation_cost, donation);
#endif
        // Take a note of the donation.
        take_note(Note(NOTE_DONATE_MONEY, donation_cost));

        you.attribute[ATTR_DONATIONS] += donation_cost;

        you.del_gold(donation_cost);

        if (donation < 1)
        {
            simple_god_message(" finds your generosity lacking.");
            return;
        }

        you.duration[DUR_PIETY_POOL] += donation;
        if (you.duration[DUR_PIETY_POOL] > 30000)
            you.duration[DUR_PIETY_POOL] = 30000;

        const int estimated_piety =
            std::min(MAX_PENANCE + MAX_PIETY,
                     you.piety + you.duration[DUR_PIETY_POOL]);

        if (player_under_penance())
        {
            if (estimated_piety >= you.penance[GOD_ZIN])
                mpr("You feel that you will soon be absolved of all your sins.");
            else
                mpr("You feel that your burden of sins will soon be lighter.");
            return;
        }

        std::string result = "You feel that " + god_name(GOD_ZIN)
                           + " will soon be ";
        result +=
            (estimated_piety > 130) ? "exalted by your worship" :
            (estimated_piety > 100) ? "extremely pleased with you" :
            (estimated_piety >  70) ? "greatly pleased with you" :
            (estimated_piety >  40) ? "most pleased with you" :
            (estimated_piety >  20) ? "pleased with you" :
            (estimated_piety >   5) ? "noncommittal"
                                    : "displeased";
        result += (donation >= 30 && you.piety <= 170) ? "!" : ".";

        mpr(result.c_str());

        return; // doesn't accept anything else for sacrifice
    }

    if (i == NON_ITEM) // nothing to sacrifice
        return;

    int num_sacced = 0;
    int num_disliked = 0;
    item_def *disliked_item = 0;

    const int old_leading = _leading_sacrifice_group();

    while (i != NON_ITEM)
    {
        item_def &item(mitm[i]);
        const int next = item.link;  // in case we can't get it later.
        const bool disliked = !god_likes_item(you.religion, item);

        if (item_is_stationary(item) || disliked)
        {
            i = next;
            if (disliked)
            {
                num_disliked++;
                disliked_item = &item;
            }
            continue;
        }

        // Ignore {!D} inscribed items.
        if (!check_warning_inscriptions(item, OPER_DESTROY))
        {
            mpr("Won't sacrifice {!D} inscribed item.");
            i = next;
            continue;
        }

        if (god_likes_item(you.religion, item)
            && (item.inscription.find("=p") != std::string::npos))
        {
            const std::string msg =
                  "Really sacrifice " + item.name(DESC_NOCAP_A) + "?";

            if (!yesno(msg.c_str(), false, 'n'))
            {
                i = next;
                continue;
            }
        }

        piety_gain_t relative_gain = PIETY_NONE;

#if DEBUG_DIAGNOSTICS || DEBUG_SACRIFICE
        mprf(MSGCH_DIAGNOSTICS, "Sacrifice item value: %d",
             item_value(item));
#endif

        for (int j = 0; j < item.quantity; ++j)
        {
            const piety_gain_t gain = _sacrifice_one_item_noncount(item);

            // Update piety gain if necessary.
            if (gain != PIETY_NONE)
            {
                if (relative_gain == PIETY_NONE)
                    relative_gain = gain;
                else            // some + some = lots
                    relative_gain = PIETY_LOTS;
            }
        }

        print_sacrifice_message(you.religion, mitm[i], relative_gain);
        item_was_destroyed(mitm[i]);
        destroy_item(i);
        i = next;
        num_sacced++;
    }

    if (num_sacced > 0 && you.religion == GOD_NEMELEX_XOBEH)
    {
        const int new_leading = _leading_sacrifice_group();
        if (old_leading != new_leading || one_chance_in(50))
            _give_sac_group_feedback(new_leading);

#if DEBUG_GIFTS || DEBUG_CARDS || DEBUG_SACRIFICE
        _show_pure_deck_chances();
#endif
    }

    if (num_sacced > 0 && you.religion == GOD_KIKUBAAQUDGHA)
    {
        simple_god_message(" torments the living!");
        torment(TORMENT_KIKUBAAQUDGHA, you.pos());
        lose_piety(random_range(8, 12));
    }

    // Explanatory messages if nothing the god likes is sacrificed.
    else if (num_sacced == 0 && num_disliked > 0)
    {
        ASSERT(disliked_item);

        if (disliked_item->base_type == OBJ_ORBS)
            simple_god_message(" wants the Orb's power used on the surface!");
        else if (is_rune(*disliked_item))
            simple_god_message(" wants the runes to be proudly displayed.");
        else if (disliked_item->base_type == OBJ_MISCELLANY
                 && disliked_item->sub_type == MISC_HORN_OF_GERYON)
            simple_god_message(" doesn't want your path blocked.");
        // Zin was handled above, and the other gods don't care about
        // sacrifices.
        else if (god_likes_fresh_corpses(you.religion))
            simple_god_message(" only cares about fresh corpses!");
        else if (you.religion == GOD_SHINING_ONE)
            simple_god_message(" only cares about unholy and evil items!");
        else if (you.religion == GOD_BEOGH)
            simple_god_message(" only cares about orcish remains!");
        else if (you.religion == GOD_NEMELEX_XOBEH)
            simple_god_message(" expects you to use your decks, not offer them!");
    }
}
