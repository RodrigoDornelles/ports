## ChavesTexto: its procedure cleared the form's seven memos and added
## every line of the game in the language asked for (1 Portuguese, any other
## Spanish). The lines are read from that code in the exe
## (platform/core/texts.c), so loading a language is choosing which to read.

import ../../platform/emu/pascal

type Memo* = enum
  MemoNomes, MemoTexto, MemoConfig, MemoFrases, MemoDialogo, MemoFinais, MemoCenarios

proc texts_line(language, memo: cint, index: csize_t): cstring {.importc, cdecl.}
proc texts_count(language, memo: cint): csize_t {.importc, cdecl.}

var idiomaCarregado*: Byte

proc CarregarTexto*(idioma: Byte) =
  idiomaCarregado := idioma

proc Lines*(memo: Memo, i: Integer): cstring =
  if i < 0: cstring("")
  else: texts_line(cint(idiomaCarregado), cint(ord(memo)), csize_t(i))

proc Count*(memo: Memo): Integer =
  Integer(texts_count(cint(idiomaCarregado), cint(ord(memo))))
