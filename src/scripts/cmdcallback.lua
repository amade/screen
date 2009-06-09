--[[ For now, this sample function will simply record all the commands executed ]]--

ticket1 = screen.hook("cmdexecuted", function(name, args)
      os.execute('mkdir -p /tmp/debug')
      local f = io.open('/tmp/debug/22', 'a')
      f:write("Command executed: " .. name)

      for i, c in pairs(args) do
	f:write(" " .. c)
      end

      f:write("\n")
      f:close()
      return 0
    end)

function cmd(name, args)
    os.execute('mkdir -p /tmp/debug')
    local f = io.open('/tmp/debug/11', 'a')
    f:write("Command executed: " .. name)

    for i, c in pairs(args) do
      f:write(" " .. c)
    end

    f:write("\n")
    f:close()
    return 0
end

ticket2 = screen.hook("cmdexecuted", "cmd")

function unhook()
  if ticket1 ~= nil then
    screen.unhook(ticket1)
    ticket1 = nil
  end

  if ticket2 ~= nil then
    screen.unhook(ticket2)
    ticket2 = nil
  end
end

