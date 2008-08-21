function find_window(name)
  display = screen.display()
  canvases = {display:get_canvases()}
  for i, c in pairs(canvases) do
    w = c.window
    if w ~= nil and (w.title == name or tostring(w.number) == name) then c:select() return end
  end

  -- Try partial matches, just like 'select'
  for i, c in pairs(canvases) do
    w = c.window
    if w ~= nil and w.title:sub(1, name:len()) == name then c:select() return end
  end

  -- We didn't find the desired window in any canvas
  -- So switch to the window in the current canvas instead
  screen.command("select " .. name)
end

