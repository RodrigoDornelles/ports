## patchs: applies line based patches on top of upstream sources (prboom,
## retroarch, lua games), so a change does not need a full copy of the file
## it touches.
##
## usage: patchs --source <upstream dir> --output <dir> <patch>...
##
## A patch file holds one or more hunks. In C (every patch but *.lua) they
## are written in documentation notation:
##
##   /**
##    * @patch libretro/libretro.c 287-290    replaces original lines 287..290
##    */
##   ...code...
##   /* @endpatch */
##
##   /** @patch libretro/libretro.c 64 */     inserts before original line 64
##   ...code...
##   /* @endpatch */
##
## and in Lua (*.lua) with --! comments, the directive anywhere in a run of
## them:
##
##   --! @patch main.lua 120-124              replaces original lines 120..124
##   --! any description
##   ...code...
##   --! @endpatch
##
## The doc comment may carry any other description; code sharing a line with
## the end marker (or, in C, with the opening comment's close) belongs to the
## hunk. Everything outside hunks is ignored. Line numbers always refer to the
## pristine upstream file, no matter how many hunks touch it; hunks that
## overlap each other are rejected instead of silently clobbering one another.
##
## Every patched file is written to <output>/<path>, the C ones with #line
## markers that point back to the upstream file and to the patch, so
## diagnostics land on the right source (Lua has nothing alike). The relative
## path of each generated file is printed on stdout, one per line.
##
## Everything but the command line works on strings only, so it builds with
## the JS backend too, for patching in the browser.

import std/[algorithm, strutils, tables]

type
  PatchError* = object of CatchableError

  Syntax* = enum
    syntaxC, syntaxLua

  Hunk* = object
    patch*: string    ## patch file the hunk came from
    directive*: int   ## line of the @patch directive
    start*: int       ## line of the first body line
    first*: int       ## first original line
    last*: int        ## last replaced line, -1 = insertion
    body*: seq[string]

  ## upstream path (relative, / separated) -> hunks in declaration order
  Hunks* = Table[string, seq[Hunk]]

proc fail(message: string) {.noreturn.} =
  raise newException(PatchError, message)

func insertion*(hunk: Hunk): bool = hunk.last < 0

func where(hunk: Hunk): string = hunk.patch & ":" & $hunk.directive

func span(hunk: Hunk): string =
  if hunk.insertion: "insertion at line " & $hunk.first
  else: "lines " & $hunk.first & "-" & $hunk.last

func syntaxOf*(path: string): Syntax =
  if path.endsWith(".lua"): syntaxLua else: syntaxC

func endText(syntax: Syntax): string =
  case syntax
  of syntaxC: "/* @endpatch */"
  of syntaxLua: "--! @endpatch"

## lines as getline reads them: the final newline ends the last line instead
## of opening an empty one, and a \r before the newline goes
func toLines*(text: string): seq[string] =
  result = text.split('\n')
  if result[^1].len == 0:
    result.setLen(result.len - 1)
  for line in result.mitems:
    if line.endsWith('\r'):
      line.setLen(line.len - 1)

func skip(text: string, at: int, chars: set[char]): int =
  result = at
  while result < text.len and text[result] in chars:
    inc result

## whether text has word as a whole word (@patch, not @patchs)
func mentions(text, word: string): bool =
  var at = text.find(word)
  while at >= 0:
    let after = at + word.len
    if after >= text.len or text[after] notin IdentChars:
      return true
    at = text.find(word, after)

type Directive = tuple[found: bool, at: int, target: string, first, last: int]

## the first well formed "@patch <file> <line>[-<line>]" of a doc comment
func parseDirective(comment: string): Directive =
  var at = comment.find("@patch")
  while at >= 0:
    let name = skip(comment, at + "@patch".len, Whitespace)
    if name > at + "@patch".len:
      let nameEnd = skip(comment, name, AllChars - Whitespace)
      let number = skip(comment, nameEnd, Whitespace)
      var stop = skip(comment, number, Digits)
      if number > nameEnd and stop > number:
        let first = parseInt(comment[number ..< stop])
        var last = -1
        let dash = skip(comment, stop, Whitespace)
        if dash < comment.len and comment[dash] == '-':
          let lastStart = skip(comment, dash + 1, Whitespace)
          let lastEnd = skip(comment, lastStart, Digits)
          if lastEnd > lastStart:
            last = parseInt(comment[lastStart ..< lastEnd])
            stop = lastEnd
        if stop == comment.len or comment[stop] in Whitespace:
          return (true, at, comment[name ..< nameEnd], first, last)
    at = comment.find("@patch", at + 1)

## where the end marker starts in text, -1 when it has none
func endMarker(text: string, syntax: Syntax): int =
  case syntax
  of syntaxC:
    var at = text.find("/*")
    while at >= 0:
      var i = skip(text, at + 2, Whitespace)
      if text.continuesWith("@endpatch", i):
        i = skip(text, i + "@endpatch".len, Whitespace)
        if text.continuesWith("*/", i):
          return at
      at = text.find("/*", at + 1)
  of syntaxLua:
    var at = text.find("--!")
    while at >= 0:
      let word = skip(text, at + 3, Whitespace)
      let after = word + "@endpatch".len
      if text.continuesWith("@endpatch", word) and (after >= text.len or text[after] notin IdentChars):
        return at
      at = text.find("--!", at + 1)
  -1

func isLuaComment(line: string): bool =
  line.strip(trailing = false).startsWith("--!")

func checkedTarget(patch: string, lineno: int, raw: string): string =
  var parts: seq[string]
  var escapes = false
  for part in raw.split('/'):
    case part
    of "", ".": discard
    of "..":
      if parts.len > 0: parts.setLen(parts.len - 1)
      else: escapes = true
    else: parts.add part
  if raw.startsWith('/') or escapes or parts.len == 0:
    fail patch & ":" & $lineno & ": error: '" & raw & "' must be relative to the upstream tree"
  parts.join("/")

## feeds one line (or the tail of one) to the open hunk; true when it was
## the end marker, which closes the hunk
proc feed(hunk: var Hunk, text: string, lineno: int, syntax: Syntax): bool =
  let marker = endMarker(text, syntax)
  if marker >= 0:
    let before = text[0 ..< marker]
    if not before.isEmptyOrWhitespace:
      if hunk.body.len == 0:
        hunk.start = lineno
      hunk.body.add before.strip(leading = false, chars = {' ', '\t'})
    return true
  if text.mentions("@patch"):
    fail hunk.patch & ":" & $lineno & ": error: @patch inside the hunk opened at line " &
      $hunk.directive & ", missing " & syntax.endText
  if hunk.body.len == 0:
    hunk.start = lineno
  hunk.body.add text

## adds the hunks of patch, whose content is text, to hunks
proc parsePatch*(hunks: var Hunks, patch, text: string) =
  let syntax = syntaxOf(patch)
  let lines = toLines(text)
  template failAt(lineno: int, what: string) =
    fail patch & ":" & $lineno & ": error: " & what

  var current: Hunk
  var target = ""
  var open = false
  template close() =
    hunks.mgetOrPut(target, @[]).add current
    open = false

  var index = 0
  while index < lines.len:
    let lineno = index + 1
    let line = lines[index]

    if open:
      if current.feed(line, lineno, syntax):
        close()
      inc index
      continue

    # gather the whole doc comment, which may span several lines; closeIndex
    # is its last line and rest the code after it on that line
    var comment = ""
    var closeIndex = index
    var rest = ""
    case syntax
    of syntaxC:
      let opening = line.find("/**")
      if opening < 0:
        if line.mentions("@endpatch"):
          failAt(lineno, "@endpatch without a matching @patch")
        inc index
        continue
      var col = opening + 3
      var closing = -1
      while closeIndex < lines.len:
        let text = lines[closeIndex]
        closing = text.find("*/", col)
        comment.add(if closing < 0: text[col .. ^1] else: text[col ..< closing])
        comment.add '\n'
        if closing >= 0:
          break
        inc closeIndex
        col = 0
      if closing < 0:
        failAt(lineno, "unterminated /** comment")
      rest = lines[closeIndex][closing + 2 .. ^1]
    of syntaxLua:
      if not line.isLuaComment:
        if line.mentions("@endpatch"):
          failAt(lineno, "@endpatch without a matching @patch")
        inc index
        continue
      # a run of --! lines; an end marker stops it, so a hunk may be empty
      while closeIndex < lines.len and lines[closeIndex].isLuaComment and
          (closeIndex == index or endMarker(lines[closeIndex], syntax) < 0):
        comment.add lines[closeIndex].strip(trailing = false)[3 .. ^1]
        comment.add '\n'
        inc closeIndex
      dec closeIndex

    let directive = parseDirective(comment)
    if not directive.found:
      if comment.mentions("@patch"):
        failAt(lineno, "malformed directive, expected '@patch <file> <line>[-<line>]'")
      if syntax == syntaxLua and comment.mentions("@endpatch"):
        failAt(lineno, "@endpatch without a matching @patch")
      index = closeIndex + 1
      continue

    let directiveLine = lineno + comment[0 ..< directive.at].count('\n')
    target = checkedTarget(patch, directiveLine, directive.target)
    current = Hunk(patch: patch, directive: directiveLine, start: closeIndex + 2,
                   first: directive.first, last: directive.last)
    open = true

    # code right after the comment, on the same line, opens the body
    if not rest.isEmptyOrWhitespace and current.feed(rest.strip, closeIndex + 1, syntax):
      close()
    index = closeIndex + 1

  if open:
    failAt(current.directive, "missing " & syntax.endText)

## rejects the hunks of target, which has count lines, that fall out of it
## or overlap each other
proc validate*(target: string, hunks: seq[Hunk], count: int) =
  for hunk in hunks:
    let limit = if hunk.insertion: count + 1 else: count
    let last = if hunk.insertion: hunk.first else: hunk.last
    if hunk.first == 0 or last < hunk.first or last > limit:
      fail hunk.where & ": error: " & hunk.span & " is out of range, " & target &
        " has " & $count & " lines"

  var replaces: seq[Hunk]
  for hunk in hunks:
    if not hunk.insertion:
      replaces.add hunk
  replaces.sort(proc (a, b: Hunk): int = cmp(a.first, b.first))

  for i in 1 ..< replaces.len:
    let (prev, next) = (replaces[i - 1], replaces[i])
    if prev.last >= next.first:
      fail next.where & ": error: " & next.span & " overlaps " & prev.span & " from " & prev.where

  # an insertion may sit right before a replaced block, never inside it
  for hunk in hunks:
    if hunk.insertion:
      for replace in replaces:
        if replace.first < hunk.first and hunk.first <= replace.last:
          fail hunk.where & ": error: " & hunk.span & " falls inside " & replace.span &
            " from " & replace.where

func quoted(path: string): string =
  result = "\""
  for c in path:
    if c in {'"', '\\'}:
      result.add '\\'
    result.add c
  result.add '"'

## target (original's path upstream) with its lines and hunks applied; the
## #line markers of C files name original and the patches as they are given
proc apply*(original, target: string, lines: seq[string], hunks: seq[Hunk]): string =
  # insertions go before a replacement starting on the same line; ties keep
  # declaration order (sort is stable)
  var hunks = hunks
  hunks.sort(proc (a, b: Hunk): int =
    if a.first != b.first: cmp(a.first, b.first)
    else: cmp(ord(not a.insertion), ord(not b.insertion)))

  let markers = syntaxOf(target) == syntaxC
  var output = ""
  var continued = false # a #line can not break a backslash continuation
  template emit(text: string) =
    output.add text
    output.add '\n'
    continued = text.endsWith('\\')
  template mark(lineno: int, file: string) =
    if markers and not continued:
      output.add "#line " & $lineno & " " & quoted(file) & "\n"

  mark(1, original)

  var next = 0
  var line = 1
  while line <= lines.len + 1:
    if next < hunks.len and hunks[next].first == line:
      let hunk = hunks[next]
      mark(hunk.start, hunk.patch)
      for text in hunk.body:
        emit(text)
      if not hunk.insertion:
        line = hunk.last + 1
      inc next
      mark(line, original)
      continue
    if line <= lines.len:
      emit(lines[line - 1])
    inc line
  output

when isMainModule and not defined(js):
  import std/[os, sets]

  proc load(path: string): string =
    try:
      readFile(path)
    except IOError:
      fail path & ": error: cannot open file"

  proc writeChanged(path, text: string) =
    if not fileExists(path) or readFile(path) != text:
      createDir(path.parentDir)
      writeFile(path, text)

  proc removeStale(output: string, keep: HashSet[string]) =
    if not dirExists(output):
      return
    # removed after the walk, not during it
    var stale: seq[string]
    for path in walkDirRec(output):
      if path notin keep:
        stale.add path
    for path in stale:
      removeFile(path)

  proc main() =
    var source, output: string
    var patches: seq[string]

    let args = commandLineParams()
    var i = 0
    while i < args.len:
      let arg = args[i]
      if arg in ["--source", "--output"] and i + 1 < args.len:
        inc i
        if arg == "--source": source = args[i] else: output = args[i]
      elif arg.startsWith("--"):
        fail "error: unknown option '" & arg & "'"
      else:
        patches.add absolutePath(arg)
      inc i
    if source.len == 0 or output.len == 0:
      fail "usage: patchs --source <upstream dir> --output <dir> <patch>..."

    # deterministic order no matter how the shell or cmake listed them
    patches.sort()

    var hunks: Hunks
    for patch in patches:
      hunks.parsePatch(patch, load(patch))

    var targets: seq[string]
    for target in hunks.keys:
      targets.add target
    targets.sort()

    var generated: HashSet[string]
    for target in targets:
      let list = hunks[target]
      let original = absolutePath(source / target)
      if not fileExists(original):
        fail list[0].where & ": error: '" & target & "' does not exist upstream"

      let lines = toLines(load(original))
      validate(target, list, lines.len)

      let destination = output / target
      writeChanged(destination, apply(original, target, lines, list))
      generated.incl destination
      stdout.writeLine target

    removeStale(output, generated)

  try:
    main()
  except CatchableError as e:
    stderr.writeLine e.msg
    quit QuitFailure
