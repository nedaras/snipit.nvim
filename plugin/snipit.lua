-- we will lazy load the plugin
vim.api.nvim_create_user_command("Snipit", function (opts)
  local snipit = require("snipit")
  if not snipit.has_setup then
    snipit.setup()
  end

  snipit.snip(opts)
end, { range = "%", nargs = "?" })
