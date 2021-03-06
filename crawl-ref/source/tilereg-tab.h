/*
 *  File:       tilereg_tab.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#ifdef USE_TILE
#ifndef TILEREG_TAB_H
#define TILEREG_TAB_H

#include "tilereg-grid.h"

// A region that contains multiple region, selectable by tabs.
class TabbedRegion : public GridRegion
{
public:
    TabbedRegion(const TileRegionInit &init);

    virtual ~TabbedRegion();

    enum
    {
        TAB_OFS_UNSELECTED,
        TAB_OFS_MOUSEOVER,
        TAB_OFS_SELECTED,
        TAB_OFS_MAX
    };

    void set_tab_region(int idx, GridRegion *reg, int tile_tab);
    GridRegion *get_tab_region(int idx);
    void activate_tab(int idx);
    int active_tab() const;
    int num_tabs() const;

    virtual void update();
    virtual void clear();
    virtual void render();
    virtual void on_resize();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
    virtual bool update_tab_tip_text(std::string &tip, bool active);
    virtual bool update_alt_text(std::string &alt);

    virtual const std::string name() const { return ""; }

protected:
    virtual void pack_buffers();
    virtual void draw_tag();
    virtual void activate() {}

    bool active_is_valid() const;
    // Returns the tab the mouse is over, -1 if none.
    int get_mouseover_tab(MouseEvent &event) const;

    int m_active;
    int m_mouse_tab;
    TileBuffer m_buf_gui;

    struct TabInfo
    {
        GridRegion *reg;
        int tile_tab;
        int ofs_y;
        int min_y;
        int max_y;
    };
    std::vector<TabInfo> m_tabs;
};

#endif
#endif
