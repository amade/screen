--[[ For now, this sample function will simply record all the commands executed ]]--
screen.hook("cmdexecuted", function(name, args)
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

screen.hook("cmdexecuted", "cmd")

