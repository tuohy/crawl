/*
 *  File:       misc.h
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
 */

#ifndef MISC_H
#define MISC_H

#include "externs.h"

struct bolt;
class dist;
struct activity_interrupt_data;

bool go_berserk(bool intentional, bool potion = false);
void search_around(bool only_adjacent = false);
void down_stairs(int old_level,
                 dungeon_feature_type force_stair = DNGN_UNSEEN,
                 entry_cause_type entry_cause = EC_UNKNOWN);
bool merfolk_change_is_safe(bool quiet = false);
void merfolk_start_swimming();
void new_level();
void trackers_init_new_level(bool transit);
void turn_corpse_into_skeleton(item_def &item);
void turn_corpse_into_chunks(item_def &item);
void turn_corpse_into_skeleton_and_chunks(item_def &item);

void init_stack_blood_potions( item_def &stack, int age = -1 );
void maybe_coagulate_blood_potions_floor( int obj );
bool maybe_coagulate_blood_potions_inv( item_def &blood );
long remove_oldest_blood_potion( item_def &stack );
void remove_newest_blood_potion( item_def &stack, int quant = -1 );
void merge_blood_potion_stacks(item_def &source, item_def &dest, int quant);

bool check_blood_corpses_on_ground();
bool can_bottle_blood_from_corpse(int mons_type);
int num_blood_potions_from_corpse(int mons_class, int chunk_type = -1);
void turn_corpse_into_blood_potions (item_def &item);
void turn_corpse_into_skeleton_and_blood_potions(item_def &item);
void split_potions_into_decay(int obj, int amount, bool need_msg = true);

void blood_spray(const coord_def& where, monster_type mon, int level);
void bleed_onto_floor(const coord_def& where, monster_type mon, int damage,
                      bool spatter = false, bool smell_alert = true);
void generate_random_blood_spatter_on_level();

// Set FPROP_BLOODY after checking bleedability.
bool maybe_bloodify_square(const coord_def& where);

bool check_annotation_exclusion_warning();
void up_stairs(dungeon_feature_type force_stair = DNGN_UNSEEN,
               entry_cause_type entry_cause = EC_UNKNOWN);

std::string weird_glowing_colour();

std::string weird_writing();

std::string weird_smell();

std::string weird_sound();

bool mons_can_hurt_player(const monsters *mon, const bool want_move = false);
bool mons_is_safe(const monsters *mon, const bool want_move = false,
                  const bool consider_user_options = true);

std::vector<monsters*> get_nearby_monsters(bool want_move = false,
                                           bool just_check = false,
                                           bool dangerous_only = false,
                                           bool consider_user_options = true,
                                           bool require_visible = true,
                                           int range = -1);

bool i_feel_safe(bool announce = false, bool want_move = false,
                 bool just_monsters = false, int range = -1);

bool there_are_monsters_nearby(bool dangerous_only = false,
                               bool require_visible = true,
                               bool consider_user_options = false);

void setup_environment_effects();

// Lava smokes, swamp water mists.
void run_environment_effects();

int str_to_shoptype(const std::string &s);

bool player_in_a_dangerous_place(bool *invis = NULL);

coord_def pick_adjacent_free_square(const coord_def& p);

int speed_to_duration(int speed);

bool scramble(void);

bool interrupt_cmd_repeat(activity_interrupt_type ai,
                          const activity_interrupt_data &at);

void reveal_secret_door(const coord_def& p);

std::string your_hand(bool plural);

bool stop_attack_prompt(const monsters *mon, bool beam_attack,
                        coord_def beam_target);

bool is_orckind(const actor *act);

bool is_dragonkind(const actor *act);
void swap_with_monster(monsters *mon_to_swap);

void maybe_id_ring_TC();
#endif
