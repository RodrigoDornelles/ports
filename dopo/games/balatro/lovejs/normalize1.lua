-- love.filesystem.newFileData(name) reached LÖVE as (name, nil), which it
-- takes as (contents, name): love.thread.newThread(file) went through it
--! @patch lua/normalize1.lua 87-87
  if arg == nil then
    return _lfs_newFile(fn)
  end
  return _lfs_newFile(fn, arg, ...)
--! @endpatch
