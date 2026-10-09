-- The generators Noterator loads, in the order the Generate panel lists them.
-- Each adapter's `panel` says where it is shown: "generate" in the Generate
-- tab, "toolbox" in the Blocks toolbox, nothing for one kept for the tests and
-- the render tool only (Midi Catalogue - decision 0017).
return {
  "adapters/good_idea.lua",
  "adapters/midi_suggester.lua",
  "adapters/midi_variator.lua",
  "adapters/starting_blocks.lua",
  "adapters/midi_catalogue.lua",
}
