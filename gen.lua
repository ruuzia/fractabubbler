#!/bin/env lua
-- Rustum Zia
-- This script takes in a path to a TTF font file and puts a bunch of glyphs
-- through the fractabubbler

local b = string.byte
function exec(s, ...)
    s = s:format(...)
    print(s)
    os.execute(s)
end

local fonts = {}
local glyphs = {}

local function item(char, name)
    glyphs[char] = name..".svg"
end

local font_path = arg[1] or "fonts/LiberationMono-Regular.ttf"

local font_name = font_path:match("([^/\\]*).ttf$")
exec("mkdir -p %q", font_name)

item(b" ", "_space")
item(b".", "_period")
item(b":", "_colon")
item(b",", "_comma")
item(b";", "_semicolon")
item(b"(", "_openparenthesis")
item(b")", "_closeparenthesis")
item(b"[", "_opensquarebrackets")
item(b"]", "_closesquarebrackets")
item(b"*", "_star")
item(b"!", "_exclamation")
item(b"?", "_question")
item(b"\'", "_singlequote")
item(b"\"", "_doublequote")

for c = b'0', b'9' do item(c, string.char(c)) end
for c = b'a', b'z' do item(c, string.char(c)) end
for c = b'A', b'Z' do item(c, string.char(c)) end

--- Execute ---
for char, name in pairs(glyphs) do
    exec("./fractabubbler --font %q --glyph %q --out %q --height 256 --fineness 4 &", font_path, char, font_name.."/"..name)
end

--- Create atlas ---
local f = assert(io.open(font_name.."/atlas", "w"))
for char, name in pairs(glyphs) do
    f:write(string.format("\n%d\n%s\n", char, name))
end
f:close()
