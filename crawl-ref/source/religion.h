/*
 *  File:       religion.h
 *  Summary:    Misc religion related functions.
 *  Written by: Linley Henzell
 */


#ifndef RELIGION_H
#define RELIGION_H

#include "enum.h"
#include "externs.h"
#include "ouch.h"
#include "player.h"

#define MAX_PIETY      200
#define HALF_MAX_PIETY (MAX_PIETY / 2)

#define MAX_PENANCE    200

enum piety_gain_t
{
    PIETY_NONE, PIETY_SOME, PIETY_LOTS,
    NUM_PIETY_GAIN
};

enum harm_protection_type
{
    HPT_NONE = 0,
    HPT_PRAYING,
    HPT_ANYTIME,
    HPT_PRAYING_PLUS_ANYTIME,
    HPT_RELIABLE_PRAYING_PLUS_ANYTIME,
    NUM_HPTS
};

bool is_evil_god(god_type god);
bool is_good_god(god_type god);
bool is_chaotic_god(god_type god);
bool is_priest_god(god_type god);
void simple_god_message(const char *event, god_type which_deity = you.religion);
int piety_breakpoint(int i);
std::string god_name(god_type which_god, bool long_name = false);
std::string god_name_jiyva(bool second_name = false);
god_type string_to_god(const char *name, bool exact = true);

std::string get_god_powers(god_type which_god);
std::string get_god_likes(god_type which_god, bool verbose = false);
std::string get_god_dislikes(god_type which_god, bool verbose = false);

void dec_penance(int val);
void dec_penance(god_type god, int val);

void excommunication(god_type new_god = GOD_NO_GOD);

void gain_piety(int pgn, bool all_at_once = false);
void dock_piety(int pietyloss, int penance);
void god_speaks(god_type god, const char *mesg);
void lose_piety(int pgn);
void handle_god_time(void);
int god_colour(god_type god);
char god_message_altar_colour(god_type god);
bool player_can_join_god(god_type which_god);
void god_pitch(god_type which_god);
int piety_rank(int piety = -1);
int piety_scale(int piety_change);
bool god_hates_your_god(god_type god,
                        god_type your_god = you.religion);
std::string god_hates_your_god_reaction(god_type god,
                                        god_type your_god = you.religion);
bool god_hates_cannibalism(god_type god);
bool god_hates_killing(god_type god, const monsters* mon);
bool god_likes_fresh_corpses(god_type god);
bool god_likes_butchery(god_type god);
bool god_likes_spell(spell_type spell, god_type god);
bool god_hates_spell(spell_type spell, god_type god);
harm_protection_type god_protects_from_harm(god_type god, bool actual = true);
bool jiyva_is_dead();
bool remove_all_jiyva_altars();
bool fedhas_protects(const monsters * target);
bool fedhas_protects_species(int mc);
bool fedhas_neutralises(const monsters * target);
bool ely_destroy_weapons();
void print_sacrifice_message(god_type, const item_def &,
                             piety_gain_t, bool = false);

bool tso_unchivalric_attack_safe_monster(const monsters *mon);

void mons_make_god_gift(monsters *mon, god_type god = you.religion);
bool mons_is_god_gift(const monsters *mon, god_type god = you.religion);

int yred_random_servants(int threshold, bool force_hostile = false);
bool is_undead_slave(const monsters* mon);
bool is_yred_undead_slave(const monsters* mon);
bool is_orcish_follower(const monsters* mon);
bool is_fellow_slime(const monsters* mon);
bool is_neutral_plant(const monsters* mon);
bool is_good_lawful_follower(const monsters* mon);
bool is_good_follower(const monsters* mon);
bool is_follower(const monsters* mon);
bool bless_follower(monsters *follower = NULL,
                    god_type god = you.religion,
                    bool (*suitable)(const monsters* mon) = is_follower,
                    bool force = false);

bool god_hates_attacking_friend(god_type god, const actor *fr);
bool god_hates_attacking_friend(god_type god, int species);
bool god_likes_item(god_type god, const item_def& item);
bool god_likes_items(god_type god);

void get_pure_deck_weights(int weights[]);

void religion_turn_start();
void religion_turn_end();

int get_tension(god_type god = you.religion, bool count_travelling = true);

bool do_god_gift(bool prayed_for = false, bool forced = false);

std::vector<god_type> temple_god_list();
std::vector<god_type> nontemple_god_list();

extern const char* god_gain_power_messages[NUM_GODS][MAX_GOD_ABILITIES];
std::string adjust_abil_message(const char *pmsg);
#endif
