import time
import screen

f = open("/tmp/debug/py", "ab")
f.write("Called at %s \n" % (time.asctime(time.localtime())))
f.close()


f = open("/tmp/debug/py", "ab")
windows = screen.windows()
for win in windows:
  f.write("Window: %s (%d) %s %s %d\n" % (win.title, win.number, win.dir, win.tty, win.pid))
f.close()
