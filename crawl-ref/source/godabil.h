/*
 *  File:       godabil.h
 *  Summary:    God-granted abilities.
 */

#ifndef GODABIL_H
#define GODABIL_H

#include "enum.h"
#include "externs.h"

class bolt;

bool is_ponderousifiable(const item_def& item);
bool ponderousify_armour();

bool zin_sustenance(bool actual = true);
bool zin_remove_all_mutations();
bool yred_injury_mirror(bool actual = true);
bool jiyva_grant_jelly(bool actual = true);
bool jiyva_remove_bad_mutation();
bool beogh_water_walk();
void yred_make_enslaved_soul(monsters *mon, bool force_hostile = false,
                             bool quiet = false, bool unrestricted = false);
bool fedhas_passthrough_class(const monster_type mc);
bool fedhas_passthrough(const monsters * target);
bool fedhas_shoot_through(const bolt & beam, const monsters * victim);
int fungal_bloom();
bool sunlight();
bool prioritise_adjacent(const coord_def &target,
                         std::vector<coord_def> &candidates);
bool plant_ring_from_fruit();
int rain(const coord_def &target);
int corpse_spores(beh_type behavior = BEH_FRIENDLY);
bool evolve_flora();
bool mons_is_evolvable(const monsters * mon);

bool vehumet_supports_spell(spell_type spell);

bool trog_burn_spellbooks();

void lugonu_bends_space();

int cheibriados_slouch(int pow);
void cheibriados_time_bend(int pow);
void cheibriados_time_step(int pow);
#endif
