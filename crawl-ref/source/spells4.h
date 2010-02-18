/*
 *  File:       spells4.h
 *  Summary:    Yet More Spell Function Declarations
 *  Written by: Josh Fishman
 */


#ifndef SPELLS4_H
#define SPELLS4_H

#include "externs.h"

class dist;
struct bolt;

bool backlight_monsters(coord_def where, int pow, int garbage);
int make_a_normal_cloud(coord_def where, int pow, int spread_rate,
                        cloud_type ctype, kill_category,
                        killer_type killer = KILL_NONE, int colour = -1,
                        std::string name = "", std::string tile = "");
int disperse_monsters(coord_def where, int pow);

void remove_condensation_shield();
void cast_condensation_shield(int pow);
void remove_divine_shield();
void cast_divine_shield();
void cast_detect_secret_doors(int pow);
void cast_discharge(int pow);
bool cast_evaporate(int pow, bolt& beem, int potion);
bool cast_fulsome_distillation(int pow);
void cast_phase_shift(int pow);
bool cast_fragmentation(int powc, const dist& spd);
bool cast_apportation(int powc, const coord_def& where);
void cast_ignite_poison(int pow);
void cast_intoxicate(int pow);
void cast_mass_sleep(int pow);
bool cast_passwall(const coord_def& delta, int pow);
bool wielding_rocks();
bool cast_sandblast(int powc, bolt &beam);
void cast_see_invisible(int pow);

void cast_shatter(int pow);
void cast_silence(int pow);
void cast_tame_beasts(int pow);
void cast_dispersal(int pow);
void cast_stoneskin(int pow);

int cast_semi_controlled_blink(int pow);
bool cast_portal_projectile(int pow);

//returns true if it slowed the monster
bool do_slow_monster(monsters* mon, kill_category whose_kill);

#endif
