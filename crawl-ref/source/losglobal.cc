#include "AppHdr.h"

#include "losglobal.h"

#include "coord.h"
#include "coordit.h"
#include "fixedarray.h"
#include "los_def.h"

typedef unsigned char losfield_t;
typedef FixedArray<losfield_t, LOS_MAX_RANGE+1, 2*LOS_MAX_RANGE+1> halflos_t;
const coord_def o_half(0, LOS_MAX_RANGE);
typedef FixedArray<halflos_t, GXM, GYM> globallos_t;

globallos_t globallos;

static losfield_t* _lookup_globallos(const coord_def& p, const coord_def& q)
{
    if (!map_bounds(p) || !map_bounds(q))
        return (NULL);
    coord_def diff = q - p;
    if (diff.rdist() > LOS_MAX_RANGE)
        return (NULL);
    // p < q iff p.x < q.x || p.x == q.x && p.y < q.y
    if (diff < coord_def(0, 0))
        return (&globallos(q)(-diff+o_half));
    else
        return (&globallos(p)(diff+o_half));
}

static void _save_los(los_def* los, los_type l)
{
    const coord_def o = los->get_center();
    for (radius_iterator ri(o, LOS_MAX_RANGE, C_SQUARE); ri; ++ri)
    {
        losfield_t* flags = _lookup_globallos(o, *ri);
        if (!flags)
            continue;
        // XXX: we're assuming that if one type of LOS is updated,
        //      all will be.
        *flags &= ~LOS_FLAG_INVALID;
        if (los->see_cell(*ri))
            *flags |= l;
        else
            *flags &= ~l;
    }
}

// Opacity at p has changed.
void invalidate_los_around(const coord_def& p)
{
    const coord_def tl = p - coord_def(LOS_MAX_RANGE, LOS_MAX_RANGE);
    const coord_def br = p + coord_def(0, LOS_MAX_RANGE);
    // We're wiping out a little more than required here.
    for (rectangle_iterator ri(tl, br); ri; ++ri)
        if (map_bounds(*ri))
            globallos(*ri).init(LOS_FLAG_INVALID);
}

void invalidate_los()
{
    for (rectangle_iterator ri(0); ri; ++ri)
        globallos(*ri).init(LOS_FLAG_INVALID);
}

static void _update_globallos_at(const coord_def& p)
{
    los_def los(p, opc_default);
    los.update();
    _save_los(&los, LOS_DEFAULT);
    los.set_opacity(opc_no_trans);
    los.update();
    _save_los(&los, LOS_NO_TRANS);
}

bool cell_see_cell(const coord_def& p, const coord_def& q, los_type l)
{
    if (l == LOS_ARENA)
        return (true);

    losfield_t* flags = _lookup_globallos(p, q);

    if (!flags)
        return (false); // outside range

    if (*flags & LOS_FLAG_INVALID)
        _update_globallos_at(p);

    ASSERT(!(*flags & LOS_FLAG_INVALID));

    return (*flags & l);
}
