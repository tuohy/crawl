/*
 *  File:       it_use2.cc
 *  Summary:    Functions for using wands, potions, and weapon/armour removal.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "it_use2.h"

#include <stdio.h>
#include <string.h>

#include "externs.h"

#include "artefact.h"
#include "beam.h"
#include "effects.h"
#include "env.h"
#include "food.h"
#include "godpassive.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "los.h"
#include "misc.h"
#include "mutation.h"
#include "player.h"
#include "religion.h"
#include "godconduct.h"
#include "skills2.h"
#include "spells2.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"
#include "tutorial.h"
#include "xom.h"

// From an actual potion, pow == 40 -- bwr
bool potion_effect(potion_type pot_eff, int pow, bool drank_it, bool was_known)
{

    bool effect = true;  // current behaviour is all potions id on quaffing

    pow = std::min(pow, 150);

    int factor = (you.species == SP_VAMPIRE
                  && you.hunger_state < HS_SATIATED
                  && drank_it ? 2 : 1);

    // Knowingly drinking bad potions is much less amusing.
    int xom_factor = factor;
    if (drank_it && was_known)
    {
        xom_factor *= 2;
        if (!player_in_a_dangerous_place())
            xom_factor *= 3;
    }

    switch (pot_eff)
    {
    case POT_HEALING:
        inc_hp((5 + random2(7)) / factor, false);
        mpr("You feel better.");

        // Only fix rot when healed to full.
        if (you.hp == you.hp_max)
        {
            unrot_hp(1);
            set_hp(you.hp_max, false);
        }

        you.duration[DUR_POISONING] = 0;
        you.rotting = 0;
        you.disease = 0;
        you.duration[DUR_CONF] = 0;
        you.duration[DUR_MISLED] = 0;
        break;

    case POT_HEAL_WOUNDS:
        inc_hp((10 + random2avg(28, 3)) / factor, false);
        mpr("You feel much better.");

        // only fix rot when healed to full
        if (you.hp == you.hp_max)
        {
            unrot_hp((2 + random2avg(5, 2)) / factor);
            set_hp(you.hp_max, false);
        }
        break;

      case POT_BLOOD:
      case POT_BLOOD_COAGULATED:
        if (you.species == SP_VAMPIRE)
        {
            // No healing anymore! (jpeg)
            int value = 800;
            if (pot_eff == POT_BLOOD)
            {
                mpr("Yummy - fresh blood!");
                value += 200;
            }
            else // Coagulated.
                mpr("This tastes delicious!");

            lessen_hunger(value, true);
        }
        else
        {
            const int value = 200;
            const int herbivorous = player_mutation_level(MUT_HERBIVOROUS);

            if (herbivorous < 3 && player_likes_chunks())
            {
                // Likes it.
                mpr("This tastes like blood.");
                lessen_hunger(value, true);

                if (!player_likes_chunks(true))
                    check_amu_the_gourmand(false);
            }
            else
            {
                mpr("Yuck - this tastes like blood.");
                if (x_chance_in_y(herbivorous + 1, 4))
                {
                    // Full herbivores always become ill from blood.
                    you.sicken(50 + random2(100));
                    xom_is_stimulated(32 / xom_factor);
                }
                else
                    lessen_hunger(value, true);
            }
        }
        did_god_conduct(DID_DRINK_BLOOD, 1 + random2(3), was_known);
        break;

    case POT_SPEED:
        if (haste_player((40 + random2(pow)) / factor))
            did_god_conduct(DID_HASTY, 10, was_known);
        break;

    case POT_MIGHT:
    {
        const bool were_mighty = you.duration[DUR_MIGHT] > 0;

        mprf(MSGCH_DURATION, "You feel %s all of a sudden.",
             were_mighty ? "mightier" : "very mighty");

        if (were_mighty)
            contaminate_player(1, was_known);
        else
            modify_stat(STAT_STRENGTH, 5, true, "");

        // conceivable max gain of +184 {dlb}
        you.increase_duration(DUR_MIGHT, (35 + random2(pow)) / factor, 80);

        did_god_conduct(DID_STIMULANTS, 4 + random2(4), was_known);
        break;
    }

    case POT_BRILLIANCE:
    {
        const bool were_brilliant = you.duration[DUR_BRILLIANCE] > 0;

        mprf(MSGCH_DURATION, "You feel %s all of a sudden.",
             were_brilliant ? "clever" : "more clever");

        if (were_brilliant)
            contaminate_player(1, was_known);
        else
            modify_stat(STAT_INTELLIGENCE, 5, true, "");

        you.increase_duration(DUR_BRILLIANCE,
                              (35 + random2(pow)) / factor, 80);

        did_god_conduct(DID_STIMULANTS, 4 + random2(4), was_known);
        break;
    }

    case POT_AGILITY:
    {
        const bool were_agile = you.duration[DUR_AGILITY] > 0;

        mprf(MSGCH_DURATION, "You feel %s all of a sudden.",
             were_agile ? "agile" : "more agile");

        if (were_agile)
            contaminate_player(1, was_known);
        else
            modify_stat(STAT_DEXTERITY, 5, true, "");

        you.increase_duration(DUR_AGILITY, (35 + random2(pow)) / factor, 80);
        you.redraw_evasion = true;

        did_god_conduct(DID_STIMULANTS, 4 + random2(4), was_known);
        break;
    }

    case POT_GAIN_STRENGTH:
        if (mutate(MUT_STRONG, true, false, false, true))
            learned_something_new(TUT_YOU_MUTATED);
        break;

    case POT_GAIN_DEXTERITY:
        if (mutate(MUT_AGILE, true, false, false, true))
            learned_something_new(TUT_YOU_MUTATED);
        break;

    case POT_GAIN_INTELLIGENCE:
        if (mutate(MUT_CLEVER, true, false, false, true))
            learned_something_new(TUT_YOU_MUTATED);
        break;

    case POT_LEVITATION:
        mprf(MSGCH_DURATION,
             "You feel %s buoyant.", !you.airborne() ? "very"
                                                           : "more");

        if (!you.airborne())
            mpr("You gently float upwards from the floor.");

        // Amulet of Controlled Flight can auto-ID.
        if (!you.duration[DUR_LEVITATION]
            && wearing_amulet(AMU_CONTROLLED_FLIGHT)
            && !extrinsic_amulet_effect(AMU_CONTROLLED_FLIGHT))
        {
            item_def& amu(you.inv[you.equip[EQ_AMULET]]);
            if (!is_artefact(amu) && !item_type_known(amu))
            {
                set_ident_type(amu.base_type, amu.sub_type, ID_KNOWN_TYPE);
                set_ident_flags(amu, ISFLAG_KNOW_PROPERTIES);
                mprf("You are wearing: %s",
                     amu.name(DESC_INVENTORY_EQUIP).c_str());
            }
        }

        // Merfolk boots unmeld if levitation takes us out of water.
        if (!you.airborne() && you.species == SP_MERFOLK
            && feat_is_water(grd(you.pos())))
        {
            unmeld_one_equip(EQ_BOOTS);
        }

        you.increase_duration(DUR_LEVITATION, 25 + random2(pow), 100);

        burden_change();
        break;

    case POT_POISON:
    case POT_STRONG_POISON:
        if (player_res_poison())
        {
            mprf("You feel %s nauseous.",
                 (pot_eff == POT_POISON) ? "slightly" : "quite" );
        }
        else
        {
            mprf(MSGCH_WARN,
                 "That liquid tasted %s nasty...",
                 (pot_eff == POT_POISON) ? "very" : "extremely" );

            poison_player( ((pot_eff == POT_POISON) ? 1 + random2avg(5, 2)
                                                    : 3 + random2avg(13, 2)) );
            xom_is_stimulated(128 / xom_factor);
        }
        break;

    case POT_SLOWING:
        if (slow_player((10 + random2(pow)) / factor))
            xom_is_stimulated(64 / xom_factor);
        break;

    case POT_PARALYSIS:
        you.paralyse(NULL,
                     (2 + random2( 6 + you.duration[DUR_PARALYSIS]
                                       / BASELINE_DELAY )) / factor);
        xom_is_stimulated(64 / xom_factor);
        break;

    case POT_CONFUSION:
        if (confuse_player((3 + random2(8)) / factor))
            xom_is_stimulated(128 / xom_factor);
        break;

    case POT_INVISIBILITY:
        if (you.haloed())
        {
            // You can't turn invisible while haloed, but identify the
            // effect anyway.
            mpr("You briefly turn translucent.");

            // And also cancel backlight (for whatever good that will
            // do).
            you.duration[DUR_CORONA] = 0;
            return (true);
        }

        if (get_contamination_level() > 0)
        {
            mprf(MSGCH_DURATION,
                 "You become %stransparent, but the glow from your "
                 "magical contamination prevents you from becoming "
                 "completely invisible.",
                 you.duration[DUR_INVIS] ? "further " : "");
        }
        else
        {
            mpr(!you.duration[DUR_INVIS] ? "You fade into invisibility!"
                                       : "You fade further into invisibility.",
            MSGCH_DURATION);
        }

        // Invisibility cancels backlight.
        you.duration[DUR_CORONA] = 0;

        // Now multiple invisiblity casts aren't as good. -- bwr
        if (!you.duration[DUR_INVIS])
            you.set_duration(DUR_INVIS, 15 + random2(pow), 100);
        else
            you.increase_duration(DUR_INVIS, random2(pow), 100);

        break;

    case POT_PORRIDGE:          // oatmeal - always gluggy white/grey?
        if (you.species == SP_VAMPIRE
            || player_mutation_level(MUT_CARNIVOROUS) == 3)
        {
            mpr("Blech - that potion was really gluggy!");
        }
        else
        {
            mpr("That potion was really gluggy!");
            lessen_hunger(6000, true);
        }
        break;

    case POT_DEGENERATION:
        if (drank_it)
            mpr("There was something very wrong with that liquid!");

        if (lose_stat(STAT_RANDOM, (1 + random2avg(4, 2)) / factor, false,
                      "drinking a potion of degeneration"))
        {
            xom_is_stimulated(64 / xom_factor);
        }
        break;

    // Don't generate randomly - should be rare and interesting.
    case POT_DECAY:
        if (you.rot(&you, (10 + random2(10)) / factor))
            xom_is_stimulated(64 / xom_factor);
        break;

    case POT_WATER:
        if (you.species == SP_VAMPIRE)
            mpr("Blech - this tastes like water.");
        else
        {
            mpr("This tastes like water.");
            lessen_hunger(20, true);
        }
        break;

    case POT_EXPERIENCE:
        if (you.experience_level < 27)
        {
            mpr("You feel more experienced!");

            you.experience = 1 + exp_needed(2 + you.experience_level);
            level_change();
        }
        else
            mpr("A flood of memories washes over you.");
        break;                  // I'll let this slip past robe of archmagi

    case POT_MAGIC:
        inc_mp((10 + random2avg(28, 3)), false);
        mpr("Magic courses through your body.");
        break;

    case POT_RESTORE_ABILITIES:
    {
        bool nothing_happens = true;
        if (you.duration[DUR_BREATH_WEAPON])
        {
            mpr("You have got your breath back.", MSGCH_RECOVERY);
            you.duration[DUR_BREATH_WEAPON] = 0;
            nothing_happens = false;
        }

        // Give a message if no message otherwise.
        if (!restore_stat(STAT_ALL, 0, false) && nothing_happens)
            mpr( "You feel refreshed." );
        break;
    }
    case POT_BERSERK_RAGE:
        if (you.species == SP_VAMPIRE && you.hunger_state <= HS_SATIATED)
        {
            mpr("You feel slightly irritated.");
            make_hungry(100, false);
        }
        else if (you.duration[DUR_BUILDING_RAGE])
        {
            you.duration[DUR_BUILDING_RAGE] = 0;
            mpr("Your blood cools.");
        }
        else
        {
            if (go_berserk(was_known, true))
                xom_is_stimulated(64);
        }
        break;

    case POT_CURE_MUTATION:
        mpr("It has a very clean taste.");
        for (int i = 0; i < 7; i++)
            if (random2(10) > i)
                delete_mutation(RANDOM_MUTATION, false);
        break;

    case POT_MUTATION:
        mpr("You feel extremely strange.");
        for (int i = 0; i < 3; i++)
            mutate(RANDOM_MUTATION, false);

        learned_something_new(TUT_YOU_MUTATED);
        did_god_conduct(DID_DELIBERATE_MUTATING, 10, was_known);
        did_god_conduct(DID_STIMULANTS, 4 + random2(4), was_known);
        break;

    case POT_RESISTANCE:
        mpr("You feel protected.", MSGCH_DURATION);
        you.increase_duration(DUR_RESIST_FIRE,   (random2(pow) + 35) / factor);
        you.increase_duration(DUR_RESIST_COLD,   (random2(pow) + 35) / factor);
        you.increase_duration(DUR_RESIST_POISON, (random2(pow) + 35) / factor);
        you.increase_duration(DUR_INSULATION,    (random2(pow) + 35) / factor);

        // Just one point of contamination. These potions are really rare,
        // and contamination is nastier.
        contaminate_player(1, was_known);
        break;

    case NUM_POTIONS:
        mpr("You feel bugginess flow through your body.");
        break;
    }

    return (effect);
}

bool unwield_item(bool showMsgs)
{
    if (!you.weapon())
        return (false);

    if (you.berserk())
    {
        if (showMsgs)
            canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    item_def& item = *you.weapon();

    const bool is_weapon = get_item_slot(item) == EQ_WEAPON;

    if (is_weapon && !safe_to_remove_or_wear(item, true))
        return (false);

    you.equip[EQ_WEAPON] = -1;
    you.wield_change     = true;
    you.m_quiver->on_weapon_changed();

    // Call this first, so that the unrandart func can set showMsgs to
    // false if it does its own message handling.
    if (is_weapon && is_artefact( item ))
        unuse_artefact(item, &showMsgs);

    if (item.base_type == OBJ_MISCELLANY
        && item.sub_type == MISC_LANTERN_OF_SHADOWS )
    {
        you.current_vision += 2;
        set_los_radius(you.current_vision);
        you.attribute[ATTR_SHADOWS] = 0;
    }
    else if (item.base_type == OBJ_WEAPONS)
    {
        const int brand = get_weapon_brand( item );

        if (brand != SPWPN_NORMAL)
        {
            const std::string msg = item.name(DESC_CAP_YOUR);

            switch (brand)
            {
            case SPWPN_FLAMING:
                if (showMsgs)
                    mprf("%s stops flaming.", msg.c_str());
                break;

            case SPWPN_FREEZING:
            case SPWPN_HOLY_WRATH:
                if (showMsgs)
                    mprf("%s stops glowing.", msg.c_str());
                break;

            case SPWPN_ELECTROCUTION:
                if (showMsgs)
                    mprf("%s stops crackling.", msg.c_str());
                break;

            case SPWPN_VENOM:
                if (showMsgs)
                    mprf("%s stops dripping with poison.", msg.c_str());
                break;

            case SPWPN_PROTECTION:
                if (showMsgs)
                    mpr("You feel less protected.");
                you.redraw_armour_class = true;
                break;

            case SPWPN_EVASION:
                if (showMsgs)
                    mpr("You feel like more of a target.");
                you.redraw_evasion = true;
                break;

            case SPWPN_VAMPIRICISM:
                if (showMsgs)
                {
                    if(you.species == SP_VAMPIRE)
                    {
                        mpr("You feel your glee subside.");
                    }
                    else
                    {
                        mpr("You feel the dreadful sensation subside.");
                    }
                }
                break;

            case SPWPN_DISTORTION:
                // Removing the translocations skill reduction of effect,
                // it might seem sensible, but this brand is supposed
                // to be dangerous because it does large bonus damage,
                // as well as free teleport other side effects, and
                // even with the miscast effects you can rely on the
                // occasional spatial bonus to mow down some opponents.
                // It's far too powerful without a real risk, especially
                // if it's to be allowed as a player spell. -- bwr

                // int effect = 9 - random2avg( you.skills[SK_TRANSLOCATIONS] * 2, 2 );
                MiscastEffect( &you, WIELD_MISCAST, SPTYP_TRANSLOCATION, 9, 90,
                               "distortion unwield" );
                break;

                // NOTE: When more are added here, *must* duplicate unwielding
                // effect in vorpalise weapon scroll effect in read_scoll.
            }

            if (you.duration[DUR_WEAPON_BRAND])
            {
                you.duration[DUR_WEAPON_BRAND] = 0;
                set_item_ego_type(item, OBJ_WEAPONS, SPWPN_NORMAL);

                // We're letting this through even if hiding messages.
                mpr("Your branding evaporates.");
            }
        }
    }
    else if (item.base_type == OBJ_STAVES && item.sub_type == STAFF_POWER)
    {
        calc_mp();
        mpr("You feel your mana capacity decrease.");
    }

    you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;

    return (true);
}

// This does *not* call ev_mod!
void unwear_armour(int slot)
{
    you.redraw_armour_class = true;
    you.redraw_evasion = true;

    item_def &item(you.inv[slot]);

    switch (get_armour_ego_type( item ))
    {
    case SPARM_RUNNING:
        mpr("You feel rather sluggish.");
        break;

    case SPARM_FIRE_RESISTANCE:
        mpr("\"Was it this warm in here before?\"");
        break;

    case SPARM_COLD_RESISTANCE:
        mpr("You catch a bit of a chill.");
        break;

    case SPARM_POISON_RESISTANCE:
        if (!player_res_poison())
            mpr("You feel less healthy.");
        break;

    case SPARM_SEE_INVISIBLE:
        if (!you.can_see_invisible())
            mpr("You feel less perceptive.");
        break;

    case SPARM_DARKNESS:        // I do not understand this {dlb}
        if (you.duration[DUR_INVIS])
            you.duration[DUR_INVIS] = 1;
        break;

    case SPARM_STRENGTH:
        modify_stat(STAT_STRENGTH, -3, false, item, true);
        break;

    case SPARM_DEXTERITY:
        modify_stat(STAT_DEXTERITY, -3, false, item, true);
        break;

    case SPARM_INTELLIGENCE:
        modify_stat(STAT_INTELLIGENCE, -3, false, item, true);
        break;

    case SPARM_PONDEROUSNESS:
        mpr("That put a bit of spring back into your step.");
        che_handle_change(CB_PONDEROUS, -1);
        break;

    case SPARM_LEVITATION:
        if (you.duration[DUR_LEVITATION])
            you.duration[DUR_LEVITATION] = 1;
        break;

    case SPARM_MAGIC_RESISTANCE:
        mpr("You feel less resistant to magic.");
        break;

    case SPARM_PROTECTION:
        mpr("You feel less protected.");
        break;

    case SPARM_STEALTH:
        mpr("You feel less stealthy.");
        break;

    case SPARM_RESISTANCE:
        mpr("You feel hot and cold all over.");
        break;

    case SPARM_POSITIVE_ENERGY:
        mpr("You feel vulnerable.");
        break;

    case SPARM_ARCHMAGI:
        mpr("You feel strangely numb.");
        break;

    case SPARM_SPIRIT_SHIELD:
        if (!player_spirit_shield())
        {
            mpr("You feel strangely alone.");
            if (you.species == SP_DEEP_DWARF)
                mpr("Your magic begins regenerating once more.");
        }
        else if (player_equip(EQ_AMULET, AMU_GUARDIAN_SPIRIT, true))
        {
            item_def& amu(you.inv[you.equip[EQ_AMULET]]);
            if (!item_type_known(amu))
            {
                set_ident_type(amu.base_type, amu.sub_type, ID_KNOWN_TYPE);
                set_ident_flags(amu, ISFLAG_KNOW_PROPERTIES);
                mprf("You are wearing: %s",
                     amu.name(DESC_INVENTORY_EQUIP).c_str());
            }
        }
        break;

    case SPARM_ARCHERY:
        mpr("Your aim is not that steady anymore.");
        break;

    default:
        break;
    }

    if (is_artefact(item))
        unuse_artefact(item);
}

void unuse_artefact(const item_def &item, bool *show_msgs)
{
    ASSERT( is_artefact( item ) );

    artefact_properties_t proprt;
    artefact_known_props_t known;
    artefact_wpn_properties( item, proprt, known );

    if (proprt[ARTP_AC])
    {
        you.redraw_armour_class = true;
        if (!known[ARTP_AC])
        {
            mprf("You feel less %s.",
                 proprt[ARTP_AC] > 0? "well-protected" : "vulnerable");
        }
    }

    if (proprt[ARTP_EVASION])
    {
        you.redraw_evasion = true;
        if (!known[ARTP_EVASION])
        {
            mprf("You feel less %s.",
                 proprt[ARTP_EVASION] > 0? "nimble" : "awkward");
        }
    }

    if (proprt[ARTP_PONDEROUS])
    {
        mpr("That put a bit of spring back into your step.");
        che_handle_change(CB_PONDEROUS, -1);
    }

    if (proprt[ARTP_MAGICAL_POWER] && !known[ARTP_MAGICAL_POWER])
    {
        mprf("You feel your mana capacity %s.",
              proprt[ARTP_MAGICAL_POWER] > 0 ? "decrease" : "increase");
    }

    // Modify ability scores; always output messages.
    modify_stat(STAT_STRENGTH,     -proprt[ARTP_STRENGTH],     false, item,
                true);
    modify_stat(STAT_INTELLIGENCE, -proprt[ARTP_INTELLIGENCE], false, item,
                true);
    modify_stat(STAT_DEXTERITY,    -proprt[ARTP_DEXTERITY],    false, item,
                true);

    if (proprt[ARTP_NOISES] != 0)
        you.attribute[ATTR_NOISES] = 0;

    if (proprt[ARTP_LEVITATE] != 0
        && you.duration[DUR_LEVITATION] > 2
        && !you.permanent_levitation())
    {
        you.duration[DUR_LEVITATION] = 1;
    }

    if (proprt[ARTP_INVISIBLE] != 0 && you.duration[DUR_INVIS] > 1)
        you.duration[DUR_INVIS] = 1;

    if (proprt[ARTP_MAGICAL_POWER])
        calc_mp();

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry *entry = get_unrand_entry(item.special);

        if (entry->unequip_func)
            entry->unequip_func(&item, show_msgs);

        if (entry->world_reacts_func)
        {
            equipment_type eq = get_item_slot(item.base_type, item.sub_type);
            you.unrand_reacts &= ~(1 << eq);
        }
    }
}
