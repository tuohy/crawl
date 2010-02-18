/*
 *  File:       dungeon.h
 *  Summary:    Functions used when building new levels.
 *  Written by: Linley Henzell
 */


#ifndef DUNGEON_H
#define DUNGEON_H

#include "fixedarray.h"
#include "externs.h"
#include "terrain.h"
#include "stuff.h"
#include "mapdef.h"

#include <vector>
#include <set>
#include <algorithm>

class map_mask;

#define BUILD_METHOD_KEY "build_method_key"
#define LAYOUT_TYPE_KEY  "layout_type_key"
#define LEVEL_VAULTS_KEY "level_vaults_key"
#define TEMP_VAULTS_KEY  "temp_vaults_key"
#define LEVEL_EXTRAS_KEY "level_extras_key"
#define LEVEL_ID_KEY     "level_id_key"

#define YOU_PORTAL_VAULT_NAMES_KEY  "you_portal_vault_names_key"

// See _build_overflow_temples() in dungeon.cc for details on overflow
// temples.
#define TEMPLE_GODS_KEY      "temple_gods_key"
#define OVERFLOW_TEMPLES_KEY "overflow_temples_key"
#define TEMPLE_MAP_KEY       "temple_map_key"

enum portal_type
{
    PORTAL_NONE = 0,
    PORTAL_LABYRINTH,
    PORTAL_HELL,
    PORTAL_ABYSS,
    PORTAL_PANDEMONIUM,
    NUM_PORTALS
};

const int MAKE_GOOD_ITEM = 351;

// Should be the larger of GXM/GYM
#define MAP_SIDE ( (GXM) > (GYM) ? (GXM) : (GYM) )

// Map mask constants.

enum map_mask_type
{
    MMT_NONE       = 0x0,
    MMT_VAULT      = 0x01,    // This is a square in a vault.
    MMT_NO_ITEM    = 0x02,    // Random items should not be placed here.
    MMT_NO_MONS    = 0x04,    // Random monsters should not be placed here.
    MMT_NO_POOL    = 0x08,    // Pool fixup should not be applied here.
    MMT_NO_DOOR    = 0x10,    // No secret-doorisation.
    MMT_NO_WALL    = 0x20,    // Wall fixup should not be applied here.
    MMT_OPAQUE     = 0x40     // Vault may impede connectivity.
};

class dgn_region;
typedef std::vector<dgn_region> dgn_region_list;

class dgn_region
{
 public:
    // pos is top-left corner.
    coord_def pos, size;

    dgn_region(int left, int top, int width, int height)
        : pos(left, top), size(width, height)
    {
    }

    dgn_region(const coord_def &_pos, const coord_def &_size)
        : pos(_pos), size(_size)
    {
    }

    dgn_region() : pos(-1, -1), size()
    {
    }

    coord_def end() const
    {
        return pos + size - coord_def(1, 1);
    }

    coord_def random_edge_point() const;
    coord_def random_point() const;

    static dgn_region absolute(int left, int top, int right, int bottom)
    {
        return dgn_region(left, top, right - left + 1, bottom - top + 1);
    }

    static dgn_region absolute(const coord_def &c1, const coord_def &c2)
    {
        return dgn_region(c1.x, c1.y, c2.x, c2.y);
    }

    static bool between(int val, int low, int high)
    {
        return (val >= low && val <= high);
    }

    bool contains(const coord_def &p) const
    {
        return contains(p.x, p.y);
    }

    bool contains(int xp, int yp) const
    {
        return (xp >= pos.x && xp < pos.x + size.x
                && yp >= pos.y && yp < pos.y + size.y);
    }

    bool fully_contains(const coord_def &p) const
    {
        return (p.x > pos.x && p.x < pos.x + size.x - 1
                && p.y > pos.y && p.y < pos.y + size.y - 1);
    }

    bool overlaps(const dgn_region &other) const;
    bool overlaps_any(const dgn_region_list &others) const;
    bool overlaps(const dgn_region_list &others,
                  const map_mask &dgn_map_mask) const;
    bool overlaps(const map_mask &dgn_map_mask) const;
};

struct vault_placement
{
public:
    coord_def pos;
    coord_def size;

    int orient;
    map_def map;
    std::vector<coord_def> exits;

    int level_number, num_runes;

    // If we're not placing runes, this is the substitute feature.
    int rune_subst;

public:
    vault_placement()
        : pos(-1, -1), size(0, 0), orient(0), map(),
          exits(), level_number(0), num_runes(0), rune_subst(-1)
    {
    }

    void reset();
    void apply_grid();
    void draw_at(const coord_def &c);
};

extern bool Generating_Level;
extern std::string dgn_Layout_Type;
extern std::string dgn_Build_Method;

extern std::set<std::string> Level_Unique_Maps;
extern std::set<std::string> Level_Unique_Tags;

extern std::vector<vault_placement> Level_Vaults;
extern std::vector<vault_placement> Temp_Vaults;

void init_level_connectivity();
void read_level_connectivity(reader &th);
void write_level_connectivity(writer &th);

bool builder(int level_number, int level_type, bool enable_random_maps = true);
void dgn_veto_level();

void dgn_flush_map_memory();

double dgn_degrees_to_radians(int degrees);
bool dgn_has_adjacent_feat(coord_def c, dungeon_feature_type feat);
coord_def dgn_random_point_in_margin(int margin);
coord_def dgn_random_point_in_bounds(
    dungeon_feature_type searchfeat,
    unsigned mapmask = MMT_VAULT,
    dungeon_feature_type adjacent = DNGN_UNSEEN,
    bool monster_free = false,
    int tries = 1500);
coord_def dgn_random_point_from(const coord_def &c, int radius, int margin = 1);
coord_def dgn_random_point_visible_from(const coord_def &c,
                                        int radius,
                                        int margin = 1,
                                        int tries = 5);
coord_def dgn_find_feature_marker(dungeon_feature_type feat);

// Generate 3 stone stairs in both directions.
void dgn_place_stone_stairs();

// Set floor/wall colour based on the mons_alloc array. Used for
// Abyss and Pan.
void dgn_set_colours_from_monsters();
void dgn_set_grid_colour_at(const coord_def &c, int colour);

bool dgn_place_map(const map_def *map, bool clobber, bool make_no_exits,
                   const coord_def &pos = coord_def(-1, -1),
                   int rune_subst = -1);

void level_clear_vault_memory();
void level_welcome_messages();

bool place_specific_trap(const coord_def& where, trap_type spec_type);
void place_spec_shop(int level_number, const coord_def& where,
                     int force_s_type, bool representative = false);
bool seen_replace_feat(dungeon_feature_type replace,
                       dungeon_feature_type feature);
bool unforbidden(const coord_def &c, unsigned mask);
coord_def dgn_find_nearby_stair(dungeon_feature_type stair_to_find,
                                coord_def base_pos, bool find_closest);

class mons_spec;
int dgn_place_monster(mons_spec &mspec,
                      int monster_level, const coord_def& where,
                      bool force_pos = false, bool generate_awake = false,
                      bool patrolling = false);

dungeon_feature_type dgn_tree_base_feature_at(coord_def c);

class item_list;
void dgn_place_multiple_items(item_list &list,
                              const coord_def& where,
                              int level);

bool set_level_flags(unsigned long flags, bool silent = false);
bool unset_level_flags(unsigned long flags, bool silent = false);

void dgn_set_lt_callback(std::string level_type_name,
                         std::string callback_name);

void dgn_reset_level(bool enable_random_maps = true);

// Returns true if the given square is okay for use by any character,
// but always false for squares in non-transparent vaults. This
// function returns sane results only immediately after dungeon generation
// (specifically, saving and restoring a game discards information on the
// vaults used in the current level).
bool dgn_square_is_passable(const coord_def &c);

void dgn_register_place(const vault_placement &place, bool register_vault);
void dgn_register_vault(const map_def &map);

struct spec_room
{
    bool created;
    bool hooked_up;

    coord_def tl;
    coord_def br;

    spec_room() : created(false), hooked_up(false), tl(), br()
    {
    }

    coord_def random_spot() const;
};

bool join_the_dots(const coord_def &from, const coord_def &to,
                   unsigned mmask, bool early_exit = false);
void spotty_level(bool seeded, int iterations, bool boxy);
void smear_feature(int iterations, bool boxy, dungeon_feature_type feature,
                   int x1, int y1, int x2, int y2);
int process_disconnected_zones(int x1, int y1, int x2, int y2,
                               bool choose_stairless,
                               dungeon_feature_type fill);
bool octa_room(spec_room &sr, int oblique_max,
               dungeon_feature_type type_floor);

int count_feature_in_box(int x0, int y0, int x1, int y1,
                         dungeon_feature_type feat);

void dgn_replace_area(const coord_def& p1, const coord_def& p2,
                      dungeon_feature_type replace,
                      dungeon_feature_type feature,
                      unsigned mmask = 0, bool needs_update = false);
void dgn_replace_area(int sx, int sy, int ex, int ey,
                      dungeon_feature_type replace,
                      dungeon_feature_type feature,
                      unsigned mmask = 0, bool needs_update = false);

void dgn_excavate(coord_def dig_at, coord_def dig_dir);
void dgn_dig_vault_loose(vault_placement &vp);
coord_def dgn_random_direction();

bool dgn_ensure_vault_placed(bool vault_success,
                             bool disable_further_vaults);

inline int count_feature_in_box( const coord_def& p1, const coord_def& p2,
                          dungeon_feature_type feat )
{
    return count_feature_in_box(p1.x, p1.y, p2.x, p2.y, feat);
}
int count_antifeature_in_box(int x0, int y0, int x1, int y1,
                             dungeon_feature_type feat);
int count_neighbours(int x, int y, dungeon_feature_type feat);
inline int count_neighbours(const coord_def& p, dungeon_feature_type feat)
{
  return count_neighbours(p.x, p.y, feat);
}

void remember_vault_placement(std::string key, vault_placement &place);

std::string dump_vault_maps();

bool dgn_square_travel_ok(const coord_def &c);

typedef std::set<dungeon_feature_type> dungeon_feature_set;
extern dungeon_feature_set dgn_Vault_Excavatable_Feats;

#endif
