---@param utf8_str string
local function snip(utf8_str)
  local max_w = 0;

  local w = 10
  local h = 20

  local x = 0
  local y = 15

  local out = ""
  -- svgs can have maximum of one space char

  for _, rune in utf8.codes(utf8_str) do
    local char = utf8.char(rune)
    assert(char ~= '\t') -- ye

    if char == ' ' then
      x = x + w
    elseif char == '\n' then
      max_w = math.max(max_w, x)

      x = 0
      y = y + h
    else
      local c = char
      if char == "<" then
        c = "&lt;"
      end

      if char == ">" then
        c = "&gt;"
      end

      if char == "&" then
        c = "&amp;"
      end

      out = out .. "<text x=\"" .. x .. "\" y=\"" .. y .. "\" class=\"monospace\">" .. c .. "</text>\n"
      x = x + w
    end

  end

  --if #line ~= 0 then
  --end

  print([[
    <svg width="]] .. max_w .. [[" height="]] .. y ..[[" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <style type="text/css">
      <![CDATA[
        .monospace {
          font-family: 'Courier New', monospace;
        }
        ]] .. "]]" ..[[>
    </style>
  </defs>
  ]] .. out .. "</svg>")
end

local input = io.read("*a")
snip(input)
