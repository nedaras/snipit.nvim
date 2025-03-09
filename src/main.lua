---@param str string
local function snip(str)
  local w = 9;
  local h = 20;
  
  print([[
    <svg width="300" height="50" xmlns="http://www.w3.org/2000/svg">
      <text x="10" y="40" font-family="Courier New, monospace" font-size="24" fill="red">]] .. str .. [[</text>
    </svg>]])
end

snip([[
---@param str string
local function snip(str)
  print(str)
end]])
