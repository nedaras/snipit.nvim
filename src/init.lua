local utf8 = require("snipit.utf8")

local M = {}

---@param str string
---@return string
local function xml(str)
  local result = ""
  for i, rune in utf8.codes(str) do
    local char = utf8.char(utf8.codepoint(rune))

    if char == "<" then
      result = result .. "&lt;"
    elseif char == ">" then
      result = result .. "&gt;"
    elseif char == "&" then
      result = result .. "&amp;"
    elseif char == "\"" then
      result = result .. "&quot;"
    elseif char == "'" then
      result = result .. "&apos;"
    else
      result = result .. char
    end
  end

  return result
end

---@param lines string[]
---@return string
local function snip(lines)
  assert(#lines ~= 0)

  local max_w = 1000;

  local w = 10
  local h = 20

  local out = ""
  for i, line in ipairs(lines) do
    if #line == 0 then
      goto continue
    end

    local x = 0
    local y = (i - 1) * h + 15

    for _, rune in utf8.codes(line) do
      local char = utf8.char(utf8.codepoint(rune))
      assert(char ~= '\n')
    end

    out = out .. "<text x=\"" .. x .. "\" y=\"" .. y .. "\" class=\"monospace\">" .. xml(line) .. "</text>"
      ::continue::
  end

  -- svgs can have maximum of one space char

  -- for _, rune in utf8.codes(utf8_str) do
    -- local char = utf8.char(utf8.codepoint(rune))


  local head = "<svg width=\"" .. max_w .. "\" height=\"" .. #lines * h .. "\" xmlns=\"http://www.w3.org/2000/svg\"><defs><style type=\"text/css\"><![CDATA[.monospace{font-family:'Courier New',monospace;}]]></style></defs>"
  return(head .. out .. "</svg>")
end


local function get_visual_selection()
  local start_line = vim.fn.line("'<")
  local end_line = vim.fn.line("'>")

  print(string.format("%d lines sniped", end_line - start_line + 1))

  return vim.fn.getline(start_line, end_line)
end

M.snip = function ()
  local mode = vim.fn.mode()
  assert(mode == 'v' or mode == 'V')

  vim.schedule(function ()
    vim.fn.setreg('+', snip(get_visual_selection()))
    --snip(get_visual_selection())
  end)

  vim.api.nvim_feedkeys(vim.api.nvim_replace_termcodes('<Esc>', true, false, true), 'v', true)
end

return M
