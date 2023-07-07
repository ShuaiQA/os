define hook-quit
  kill
end
# source script/debug.py
# set disassemble-next-line on
target remote : 1234
b main
c
