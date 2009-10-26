------------------------------------------------------------------------------
-- loadmaps.lua:
--
-- Compiles and loads .des files that Crawl needs. This only includes the
-- base .des files. Optional .des files that the user requests in .crawlrc
-- are handled separately.
------------------------------------------------------------------------------

local des_files = {
  -- The dummy vaults that define global vault generation odds.
  "dummy.des",

  -- Example vaults, included here so that Crawl will syntax-check them
  "didact.des",

  "arena.des",

  "zotdef.des"
}

for _, file in ipairs(des_files) do
   dgn.load_des_file(file)
end
