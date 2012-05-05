import screen

def input_cb(str):
  f = open("/tmp/debug/py", "ab")
  f.write("%s\n" % str)
  f.close()

screen.input("Test:", input_cb, "123")

