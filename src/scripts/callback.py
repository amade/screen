import screen

ticket = None

def cmd_cb(event, params):
  f = open("/tmp/debug/py", "ab")
  f.write("Event triggered: %s (%s)\n" % (event, params))
  f.close()
  return 0

ticket = screen.hook("global_cmdexecuted", cmd_cb)

def detached_cb(display, flags):
  ticket.unhook()
  return 0

screen.hook("global_detached", detached_cb)

