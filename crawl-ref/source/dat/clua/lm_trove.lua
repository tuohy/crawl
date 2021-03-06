------------------------------------------------------------------------------
-- lm_trove.lua:
-- Custom (temporarily) timed, and tolled (gold or item) map marker.
--  Basically just a combination of TimedMarker and TollStair, with added veto
--  checks against items.
--
-- Current only accepts the following:
--  any armour
--  any jewellery
--  any weapon
--  any book
--  any scroll
--  any potion
--  any wand
------------------------------------------------------------------------------

require('clua/lm_tmsg.lua')
require('clua/lm_1way.lua')

TroveMarker = util.subclass(OneWayStair)
TroveMarker.CLASS = "TroveMarker"

function TroveMarker:new(props)
  -- On top of the normal Toll stair (the 'amount' value), this also accepts
  -- secondary_amount and toll_item.
  props = props or { }

  local tmarker = self.super.new(self, props)

  if not props.msg then
    error("No messaging object provided (msg = nil)")
  end

  props.high = props.high or props.low or props.turns or 1
  props.low  = props.low or props.high or props.turns or 1

  local dur = crawl.random_range(props.low, props.high, props.navg or 1)
  if fnum == dgn.feature_number('unseen') then
    error("Bad feature name: " .. feat)
  end

  tmarker.dur = dur * 10
  tmarker.msg = props.msg

  if not props.amount or props.amount < 1 then
    error("Bad toll amount: " .. props.amount)
  end

  if not props.secondary_amount or props.secondary_amount < 1 then
    error("Bad second toll amount: " .. props.secondary_amount)
  end

  if not props.toll_item or props.toll_item == nil then
    error("Need a toll item.")
  end

  tmarker.seen = false
  tmarker.no_timeout = false
  item = props.toll_item
  if item.quantity == nil then
    error("Item needs a quantity.")
  end
  if item.base_type == nil then
    error("Item needs a base type.")
  end
  if item.sub_type == nil then
    error("Item needs a sub type.")
  end
  if item.ego_type == nil or item.ego_type == "" then
    item.ego_type = false
  end
  if item.plus1 == nil then
    item.plus1 = false
  end
  if item.plus2 == nil then
    item.plus2 = false
  end
  if item.artefact_name == nil then
    item.artefact_name = false
  end

  tmarker.toll_item = item

  props.msg = nil

  return tmarker
end

function TroveMarker:activate(marker, verbose)
  self.super.activate(self, marker, verbose)
  self.msg:init(self, marker, verbose)
  dgn.register_listener(dgn.dgn_event_type('turn'), marker)
  dgn.register_listener(dgn.dgn_event_type("v_leave_level"),
                        marker, marker:pos())
  dgn.register_listener(dgn.dgn_event_type("player_los"),
                        marker, marker:pos())
end

function TroveMarker:fdesc_long (marker)
  local state = ""
  if self.no_timeout then
    state = "This portal is broken and requires " ..
            self:item_name() .. " to function.\n"
  else
    state = "This portal will remain here for a" ..
             self.props.dur .. " time.\n"
  end
  return state .. "\n" .. self.props.desc_long
end

function TroveMarker:overview_note (marker)
  if self.no_timeout then
    return self:item_name(false)
  else
    return "" .. self.props.secondary_amount .. " gold/" ..
           self.props.amount .. " gold"
  end
end

function TroveMarker:debug_portal (marker)
  local _x, _y = marker:pos()
  if #dgn.items_at(_x, _y) ~= 0 then
    new_item = dgn.items_at(_x, _y)[1]
    if crawl.yesno("Switch Trove to " .. new_item.name() .. " instead?", true, "n") then
      self.toll_item = trove.get_trove_item(nil, 0, new_item)
      crawl.mpr("Switched to a new item.")
    else
      crawl.mpr("Ignoring " .. new_item.name())
    end
  end

  if crawl.yesno("Run diagnostic on inventory?", true, "n") then
    self:check_item(marker, nil, "inventory", true)
  elseif crawl.yesno("Run diagnostic on floor?", true, "n") then
    local _x, _y = marker:pos()
    self:check_item(marker, nil, dgn.point(_x, _y), true)
  end

  local item = self.toll_item
  if item == nil then
    return "Failed to get item???"
  end

  if item.quantity == nil then item.quantity = false end
  if item.base_type == nil then item.base_type = false end
  if item.sub_type == nil then item.sub_type = false end
  if item.ego_type == nil then item.ego_type = false end
  if item.plus1 == nil then item.plus1 = false end
  if item.plus2 == nil then item.plus2 = false end
  if item.artefact_name == nil then item.artefact_name = false end

  if item.quantity ~= false then
    crawl.mpr("Wanted quantity: " .. item.quantity)
  else
    crawl.mpr("Wanted quantity: nil.")
  end

  if item.base_type ~= false then
    crawl.mpr("Wanted base type: " .. item.base_type)
  else
    crawl.mpr("Wanted base type: nil")
  end

  if item.sub_type ~= false then
    crawl.mpr("Wanted sub type: " .. item.sub_type)
  else
    crawl.mpr("Wanted sub type: nil")
  end

  if item.ego_type ~= false then
    crawl.mpr("Wanted ego type: " .. item.ego_type)
  else
    crawl.mpr("Wanted ego type: nil")
  end

  if item.plus1 ~= false then
    crawl.mpr("Wanted plus1: " .. item.plus1)
  else
    crawl.mpr("Unwanted plus1.")
  end

  if item.plus2 ~= false then
    crawl.mpr("Wanted plus2: " .. item.plus2)
  else
    crawl.mpr("Unwanted plus2.")
  end

  if item.artefact_name ~= false then
    crawl.mpr("Wanted artefact name: " .. item.artefact_name)
  else
    crawl.mpr("Unwanted artefact.")
  end

  return "Done. Item name is " .. self:item_name() .. "!"
end

function TroveMarker:property(marker, pname)
  if pname == 'veto_stair' then
    return self:check_veto(marker, pname)
  end

  if pname == "feature_description_long" then
    return self:fdesc_long (marker)
  elseif pname == "overview_note" then
    return self:overview_note (marker)
  end

  if pname == "portal_debug" then
    return self:debug_portal (marker)
  end

  return self.super.property(self, marker, pname)
end

function TroveMarker:disappear(marker, affect_player, x, y)
  dgn.remove_listener(marker)
  self.super.disappear(self, marker, affect_player, x, y)
end

function TroveMarker:timeout(marker, verbose, affect_player)
  local x, y = marker:pos()

  if you.pos() == marker:pos() and you.taking_stairs() then
    if verbose then
      crawl.mpr( dgn.feature_desc_at(x, y, "The") .. " vanishes " ..
                "just as you enter it!")
      return
    end
  end

  if verbose then
    if you.see_cell(marker:pos()) then
      crawl.mpr( util.expand_entity(self.props.entity, self.props.disappear) or
                 dgn.feature_desc_at(x, y, "The") .. " disappears!")
    else
      crawl.mpr("The walls and floor vibrate strangely for a moment.")
    end
  end

  self:disappear(marker, affect_player, x, y)
end

function TroveMarker:event(marker, ev)
  local x, y = ev:pos()
  if ev:type() == dgn.dgn_event_type('player_los') and self.seen ~= true then
    self.seen = true
  end

  if self.super.event(self, marker, ev) then
    return true
  end

  self.ticktype = self.ticktype or dgn.dgn_event_type('turn')

  if ev:type() ~= self.ticktype then
    return
  end

  if self.no_timeout then
    return
  end

  self.dur = self.dur - ev:ticks()
  self.msg:event(self, marker, ev)
  if self.dur <= 0 then
    self:timeout(marker, true, true)
    return true
  end
end

function TroveMarker:describe(marker)
  local desc = self:unmangle(self.props.desc_long)
  if desc then
    desc = desc .. "\n\n"
  else
    desc = ""
  end
  return desc .. "The portal charges " .. self.props.amount .. " for entry.\n"
end

function TroveMarker:read(marker, th)
  TroveMarker.super.read(self, marker, th)
  self.seen = file.unmarshall_meta(th)
  self.no_timeout = file.unmarshall_meta(th)
  self.toll_item = lmark.unmarshall_table(th)
  self.dur  = file.unmarshall_number(th)
  self.msg  = file.unmarshall_fn(th)(th)

  setmetatable(self, TroveMarker)
  return self
end

function TroveMarker:write(marker, th)
  TroveMarker.super.write(self, marker, th)
  file.marshall_meta(th, self.seen)
  file.marshall_meta(th, self.no_timeout)
  lmark.marshall_table(th, self.toll_item)
  file.marshall(th, self.dur)
  file.marshall(th, self.msg.read)
  self.msg:write(th)
end

function TroveMarker:item_name(do_grammar)
  local item = self.toll_item
  local s = ""
  if item.quantity > 1 then
    s = s .. item.quantity
  end

  if item.artefact_name ~= false then
    if string.find(item.artefact_name, "'s") or do_grammar == false then
      return item.artefact_name
    else
      return "the " .. item.artefact_name
    end
  end

  if item.base_type == "weapon" or item.base_type == "armour" then
    if item.plus1 ~= false and item.plus1 ~= nil then
      s = s .. " "
      if item.plus1 > -1 then
        s = s .. "+"
      end
      s = s .. item.plus1
    end

    if item.plus2 ~= false and item.plus2 ~= nil then
      if item.plus1 ~= item.plus2 and item.base_type ~= "armour" then
        s = s .. ", +"
        if item.plus2 > -1 then
          s = s .. item.plus2
        end
      end
    end
  end

  local jwith_pluses = {"ring of protection", "ring of evasion",
                        "ring of strength", "ring of intelligence",
                        "ring of dexterity", "ring of slaying"}
  if item.base_type == "jewellery" and
     util.contains(jwith_pluses, item.sub_type) then
    s = s .. " +" .. item.plus1
    if item.sub_type == "ring of slaying" and item.plus2 ~= item.plus1 then
      s = s .. ", +" .. item.plus2
    end
  end

  if item.base_type == "potion" or item.base_type == "scroll" then
    if item.quantity > 1 then
      s = s .. " " .. item.base_type .. "s of"
    else
      s = s .. " " .. item.base_type .. " of"
    end
  elseif item.base_type == "book" then
    books = {"Necronomicon", "tome of Destruction",
             "Young Poisoner's Handbook", "Monster Manual"}
    if not string.find(item.sub_type, "manual")
       and not util.contains(books, item.sub_type) then
      s = s .. " book of"
    end
  elseif item.base_type == "wand" then
    s = s .. " wand of"
  end

  s = s .. " " .. item.sub_type

  if item.base_type == "wand" then
    s = s .. " (" .. item.plus1 .. ")"
  end

  if item.base_type == "armour" or item.base_type == "weapon" then
    if item.ego_type then
      s = s .. " of " .. item.ego_type
    end
  end

  s = util.trim(s)

  if string.find(s, "^%d+") or do_grammar == false then
    return s
  else
    return crawl.grammar(s, "a")
  end
end

-- Returns true if the item was found and "used", false
-- if we didn't find anything.
function TroveMarker:check_item(marker, pname, position, dry_run)
  local iter_table
  local acceptable_items = {}

  if position == "inventory" then
    if dry_run ~= nil then crawl.mpr("Checking inventory.") end
    iter_table = items.inventory()
  else
    if dry_run ~= nil then crawl.mpr("Checking " .. position.x .. "/" .. position.y) end
    iter_table = dgn.items_at(position.x, position.y)
  end

  if #iter_table == 0 then
    if dry_run ~= nil then crawl.mpr("No items.") end
    return false
  end

  local item = self.toll_item

  for it in iter.invent_iterator:new(iter_table) do
    local iplus1, iplus2 = it.pluses()
    local this_item = true
    if dry_run ~= nil then crawl.mpr("Checking item: " .. it.name()) end

    if not it.identified("type properties pluses") then
      if dry_run ~= nil then crawl.mpr("Item not identified.") end
      this_item = false
    end

    if it.artefact == true and item.artefact_name == false then
      if dry_run ~= nil then crawl.mpr("Item artefact doesn't match.") end
      this_item = false
    end

    local jwith_pluses = {"ring of protection", "ring of evasion",
                        "ring of strength", "ring of intelligence",
                        "ring of dexterity", "ring of slaying"}
    if it.base_type == "jewellery" then
      if not util.contains(jwith_pluses, it.sub_type) then
         iplus1 = false
         iplus2 = false
      elseif it.sub_type ~= "ring of slaying" then
         iplus2 = false
      end
    end

    if iplus1 == false then
      iplus1 = nil
    end

    if iplus2 == false then
      iplus2 = nil
    end

    if iplus1 == nil or iplus2 == nil then
      if item.plus1 ~= false or item.plus2 ~= false then
        if dry_run ~= nil then crawl.mpr("Nil pluses when we want pluses.") end
        this_item = false
      end
    end

    -- And don't do anything if the pluses aren't correct, either.
    -- but accept items that are plus'd higher than spec'd.
    if iplus1 ~= nil and item.plus1 ~= false and iplus1 < item.plus1 then
      if dry_run ~= nil then crawl.mpr("Pluses do not match: " .. item.plus1 .. " versus " .. iplus1) end
      this_item = false
    elseif iplus2 ~= nil and item.plus2 ~= false and iplus2 < item.plus2 then
      if dry_run ~= nil then crawl.mpr("Pluses do not match: " .. item.plus2 .. " versus " .. iplus2) end
      this_item = false
    end

    if it.quantity == nil then
      if dry_run ~= nil then crawl.mpr("Quantity is nil.") end
      this_item = false
    elseif item.quantity > 1 and it.quantity < item.quantity then
      if dry_run ~= nil then crawl.mpr("Quantity does not match, want " .. item.quantity .. ", got " .. it.quantity) end
      this_item = false
    end

    if item.artefact_name then
      if item.artefact_name ~= it.artefact_name then
        if dry_run ~= nil then crawl.mpr("Artefact name does not match, want " .. item.artefact_name .. ", got " .. it.artefact_name) end
        this_item = false
      end
    end

    if item.ego_type then
      if item.ego_type ~= it.ego_type then
        if dry_run ~= nil then crawl.mpr("Ego type does not match, want " .. item.ego_type .. ", got " .. it.ego_type) end
        this_item = false
      end
    end

    -- Now all we need to do is to make sure that the item
    -- is the one we're looking for
    if this_item and item.sub_type == it.sub_type
       and item.base_type == it.base_type then
       if dry_run ~= nil then crawl.mpr("Accepting " .. it.name() .. " and added to queue.") end
      table.insert(acceptable_items, it)
    elseif this_item then
      if dry_run ~= nil then crawl.mpr("Sub and base types do not match.") end
      if dry_run ~= nil then crawl.mpr("Wanted base type " .. item.base_type .. ", got " .. it.base_type) end
      if dry_run ~= nil then crawl.mpr("Wanted sub type " .. item.sub_type .. ", got " .. it.sub_type) end
    end
  end

  if #acceptable_items == 0 then
    if dry_run ~= nil then crawl.mpr("No acceptable items.") end
    return false
  end

  --crawl.mpr("Total of " .. #acceptable_items .. " acceptable items.")

  -- If there are no pluses but multiple acceptable items, or there is only one
  -- acceptable item, take the first in the list.
  if #acceptable_items == 1
     or (item.plus1 == false and item.plus2 == false) then
    local it = acceptable_items[1]
    if dry_run == nil then
      return self:accept_item(it)
    end
  else
    -- Otherwise, take the one with the least pluses.
    local titem = acceptable_items[1]
    local titem_p1, titem_p2 = titem.pluses()
    --crawl.mpr("Picking " .. titem.name() .. " to start with.")
    --crawl.mpr("This item p1: " .. titem_p1 .. ", p2: " .. titem_p2)

    if item.plus2 == false then
      --crawl.mpr("Not looking at second plus.")
    end

    for _, it in ipairs(acceptable_items) do
      local this_p1, this_p2 = it.pluses()
      --crawl.mpr("Looking at " .. it.name())
      if this_p1 < titem_p1 and (item.plus2 == false or this_p2 < titem_p2) then
        titem = it
        titem_p1, titem_p2 = titem.pluses()
        --crawl.mpr("Picking " .. titem.name() ..
        --          " instead (lesser pluses instad)")
        --crawl.mpr("This item p1: " .. titem_p1 .. ", p2: " .. titem_p2)
      elseif this_p1 == item.plus1 and this_p2 == item.plus2 then
        titem = it
        titem_p1, titem_p2 = titem.pluses()
        --crawl.mpr("Picking " .. titem.name() ..
        --          " instead (matches wanted pluses)")
        --crawl.mpr("This item p1: " .. titem_p1 .. ", p2: " .. titem_p2)
      end
    end

    if dry_run == nil then
      return self:accept_item(titem)
    end
  end
end

function TroveMarker:accept_item (it)
  crawl.mpr("The portal accepts the item" .. self:plural() ..
            " and buzzes to life!")
  it.dec_quantity(self.props.toll_item.quantity)
  return true
end

function TroveMarker:plural ()
  if self.toll_item.quantity > 1 then
    return "s"
  else
    return ""
  end
end

function TroveMarker:check_veto(marker, pname)
  -- Have we enough gold?
  local gold = you.gold()
  local needed = self.props.amount
  local needed2 = self.props.secondary_amount
  local quantity = false
  local plus = 0

  if gold < needed2 and self.no_timeout == false then
    crawl.mpr("This portal charges a minimum of " .. needed2 ..
              " gold for entry; " .. "you have only " .. gold .. " gold.")
    return "veto"
  end

  local _x, _y = marker:pos()

  if pname == "veto_stair" then
    -- Let's see if they want to pay the smaller amount
    if self.no_timeout == true then
      -- Let's check to see if there's an item at our current position
      if crawl.yesno("This trove needs " .. self:item_name() ..
                     " to function. Give it the item" ..
                     self:plural() .. "?", true, "n") then
        if self:check_item(marker, pname, dgn.point(_x, _y)) == true then
          return
        else
          if self:check_item(marker, pname, "inventory") == true then
            self:note_payed(self:item_name())
            return
          else
            crawl.mpr("You don't have the item" .. self:plural() ..
                      " to give!")
            return "veto"
          end
        end
      end
      return "veto"
    else
      crawl.mpr("There is a foul-smelling imp just inside the portal. " ..
                "It says, \"I can lock this portal here for " ..
                needed2 .."gp!\".")
      if crawl.yesno("Pay? (you can refuse and instead pay the full fee of " ..
                     needed .. "gp to enter now)", true, "n") then
        self.no_timeout = true
        you.gold(you.gold() - needed2)
        self:note_payed(needed2 .. "gp", true)
        crawl.mpr("The imp fiddles with the portal and frowns. " ..
                  "It says, " ..
                  "\"I think I broke it! You might be able to fix it with " ..
                  self:item_name() .. ".\" " ..
                  "before disappearing in a puff of smoke.")
        return "veto"
      else
        crawl.mpr("The foul-smelling imp curses your mother!")
      end

      -- Ok, ask if the player wants to spend the $$$.
      if crawl.yesno("You can still enter the portal, but it charges " ..
                     needed .. "gp for entry. Pay?", true, "n") then
        if you.gold() < needed then
          crawl.mpr("You don't have the money to pay for it!")
          return "veto"
        else
          you.gold(you.gold() - needed)
          self:note_payed(needed .. "gp")
        end
      else
        return "veto"
      end
    end
  end
end

function TroveMarker:note_payed(amount, lock)
  local toll_desc
  if self.props.toll_desc then
      toll_desc = self.props.toll_desc
  else
      toll_desc = "at " .. crawl.article_a(self.props.desc)
  end

  local tdesc = "toll"

  if lock ~= nil then
    tdesc = "fee"
    if self.props.lock_desc then
      toll_desc = self.props.lock_desc
    end
  end


  crawl.take_note("You paid a " .. tdesc .. " of " .. amount .. " " ..
                  toll_desc .. ".")
end

function trove_marker(pars)
  return TroveMarker:new(pars)
end
