local utf8 = require("snipit.utf8")

-- using zig we need to make an interface where we could draw an image

local M = {}

---@param str string
---@return string
local function xml(str)
  local result = ""
  for _, rune in utf8.codes(str) do
    local char = utf8.char(utf8.codepoint(rune))

    if char == "<" then
      result = result .. "&lt;"
    elseif char == ">" then
      result = result .. "&gt;"
    elseif char == "&" then
      result = result .. "&amp;"
    --elseif char == "\"" then
      --result = result .. "&quot;"
    --elseif char == "'" then
      --result = result .. "&apos;"
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

    local y = i * h - 5

    local prev_rune = nil
    local spaces = 0

    local prev_idx = nil
    for indx, codes in utf8.codes(line) do
      local rune = utf8.char(utf8.codepoint(codes))
      assert(rune ~= '\n')

      if rune == ' ' then
        spaces = spaces + 1
      else
        if prev_rune ~= nil and prev_rune == ' ' and spaces > 1 then
          if prev_idx then
            local x = prev_idx * w
            local sub_line = string.sub(line, prev_idx, indx - spaces - 1)

            out = out .. "<text x=\"" .. x .. "\" y=\"" .. y .. "\" class=\"monospace\">" .. xml(sub_line) .. "</text>"
          end
          prev_idx = indx
        end
        spaces = 0
      end
      prev_rune = rune
    end

    if prev_idx then
      local x = prev_idx * w
      local sub_line = string.sub(line, prev_idx)
      assert(#sub_line ~= 0)

      out = out .. "<text x=\"" .. x .. "\" y=\"" .. y .. "\" class=\"monospace\">" .. xml(sub_line) .. "</text>"
    else
      out = out .. "<text x=\"0\" y=\"" .. y .. "\" class=\"monospace\">" .. xml(line) .. "</text>"
    end


    ::continue::
  end

  local head = "<svg width=\"" .. max_w .. "\" height=\"" .. #lines * h .. "\" xmlns=\"http://www.w3.org/2000/svg\"><defs><style>.monospace{font-family:'Courier New',monospace;}></style></defs>"
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
  end)

  vim.api.nvim_feedkeys(vim.api.nvim_replace_termcodes('<Esc>', true, false, true), 'v', true)
end

M.test = function (start_row, end_row)
  local buf = vim.api.nvim_get_current_buf()
  local highlighter = vim.treesitter.highlighter
  if highlighter.active[buf] then
    local buf_highlighter = highlighter.active[buf]

    buf_highlighter.tree:for_each_tree(function (tstree, tree)
      if not tstree then
        return
      end

      local root = tstree:root()
      -- local root_start_row, _, root_end_row, _ = root:range()

      local query = buf_highlighter:get_query(tree:lang())

      if not query:query() then
        return
      end

      local iter = query:query():iter_captures(root, buf_highlighter.bufnr, start_row - 1, end_row)

      local out = ""
      local max_x = 0
      local max_y = 0
      -- todo: handle duplicates
      for capture, node in iter do
        local hl = query.hl_cache[capture]
        if hl then
          local hl_group = query._query.captures[capture]
          local hl_info = vim.api.nvim_get_hl_by_name("@" .. hl_group, true)
          if hl_info and hl_info.foreground then
            local str = vim.treesitter.get_node_text(node, buf)
            local y, x, _, _ = node:range()
            local color = string.format("#%06x", hl_info.foreground)

            max_x = math.max(max_x, (x + #str) * 10)
            max_y = math.max(max_y, y * 20)
            out = out .. "<text x=\"" .. x * 10 .. "\" y=\"" .. y * 20 + 15 .. "\" fill=\"" .. color.. "\" class=\"monospace\">" .. xml(str) .. "</text>"
          end
          -- print(string.format("%s: `%s` [%d:%d - %d:%d] ", hl_group, name, sr, sc, er, ec))
          -- print(color)
        end
      end


      local head = "<svg width=\"" .. max_x .. "\" height=\"" .. max_y + 20 .. "\" xmlns=\"http://www.w3.org/2000/svg\"><defs><style>.monospace{font-family:'Courier New',monospace;}></style></defs>"
      local rect = "<rect width=\"100%\" height=\"100%\" fill=\"#191724\"></rect>"
      vim.fn.setreg('+', head .. rect .. out .. "</svg>")
    end, true)
  end
end

return M
