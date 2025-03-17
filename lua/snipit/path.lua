local Path = {}

---@param path string
---@return boolean
local function is_absolute_windows(path)
  if #path < 1 then
    return false
  end

  if path:sub(1, 1) == '/' then
    return true
  end

  if path:sub(1, 1) == '\\' then
    return true
  end

  if path:sub(2, 2) == ':' then
    if (path:sub(3, 3) == '/') then
      return true
    end

    if (path:sub(3, 3) == '\\') then
      return true
    end
  end

  return false
end

---@param path string
---@return boolean
local function is_absolute_posix(path)
  return path:sub(1, 1) == '/'
end

---@param path string
---@return boolean
Path.is_absolute = function(path)
  if jit.os == "Windows" then
    return is_absolute_windows(path)
  end
  return is_absolute_posix(path)
end

---@param path string
---@return string 
local function disk_designator_windows(path)
  if path:sub(2, 2) == ':' then
    return path:sub(1, 2)
  end

  if (path:sub(1, 1) == '/' or path:sub(1, 1) == '\\') and
    (#path == 1 or (path:sub(2, 2) ~= '/' and path:sub(2, 2) ~= '\\'))
  then
    return ""
  end

  if #path < #"//a/b" then
    return ""
  end

  for _, this_sep in pairs({ '/', '\\' }) do
    local two_steps = this_sep .. this_sep
    if path:sub(1, 2) == two_steps then
      if path:sub(3, 3) == this_sep then
        return ""
      end

      local idx = 1
      for _ = 1, 2 do
        while path:sub(idx, idx) == this_sep do
          idx = idx + 1
        end

        if idx == #path - 1 then
          return ""
        end

        while idx < #path + 1 and path:sub(idx, idx) ~= this_sep do
          idx = idx + 1
        end
      end

      return path:sub(1, idx)
    end
  end

  return ""
end

---@param path string
---@return nil|string 
local function dirname_windows(path)
  if #path == 0 then
    return nil
  end

  local root_slice = disk_designator_windows(path)
  if #path == #root_slice then
    return nil
  end

  local have_root_slash = #path > #root_slice and (path:sub(#root_slice + 1, #root_slice + 1) == '/' or path:sub(#root_slice + 1, #root_slice + 1) == '\\')
  local end_index = #path

  while path:sub(end_index, end_index) == '/' or path:sub(end_index, end_index) == '\\' do
    if end_index == 1 then
      return nil
    end
    end_index = end_index - 1
  end

  while path:sub(end_index, end_index) ~= '/' and path:sub(end_index, end_index) ~= '\\' do
    if end_index == 1 then
      return nil
    end
    end_index = end_index - 1
  end

  if have_root_slash and end_index == #root_slice + 1 then
    end_index = end_index + 1
  end

  if end_index == 1 then
    return nil
  end

  return path:sub(1, end_index - 1)
end

---@param path string
---@return nil|string 
local function dirname_posix(path)
  if #path == 0 then
    return nil
  end

  local end_index = #path
  while path:sub(end_index, end_index) == '/' do
    if end_index == 1 then
      return nil
    end
    end_index = end_index - 1
  end

  while path:sub(end_index, end_index) ~= '/' do
    if end_index == 1 then
      return nil
    end
    end_index = end_index - 1
  end

  if end_index == 1 and path:sub(1, 1) == '/' then
    return "/"
  end

  if end_index == 1 then
    return nil
  end

  return path:sub(1, end_index - 1)
end

---@param path string
---@return nil|string 
Path.dirname = function(path)
  if jit.os == "Windows" then
    return dirname_windows(path)
  end
  return dirname_posix(path)
end

return Path
