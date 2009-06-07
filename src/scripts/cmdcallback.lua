--[[ For now, this sample function will simply record all the commands executed ]]--
function command_executed(name, args)
  os.execute('mkdir -p /tmp/debug')
  local f = io.open('/tmp/debug/ll', 'a')
  f:write("Command executed: " .. name)

  for i, c in pairs(args) do
    f:write(" " .. c)
  end

  f:write("\n")
  f:close()
end

function toogle_cmd_log()
  if  (type(ticket) == "nil") then
    ticket = screen.hook("cmdexecuted", "command_executed")
  else
    screen.unhook(ticket, "command_executed")
    ticket = nil
  end
end
