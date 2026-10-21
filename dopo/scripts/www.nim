## www: renders a page of the site from a mustache template, with the
## partials of each --partials dir: the ones every page shares (head,
## header), then the page's own.
##
## usage: www --partials <dir>... --template <page.html> --output <file>
##            [--data <file.json>]... [key=value]...
##
## The values are the keys of each --data JSON object, then each key=value,
## later ones over earlier ones. The output is only rewritten on change.

import std/[json, os, strutils]
import mustache

proc fail(message: string) {.noreturn.} =
  stderr.writeLine "www: ", message
  quit QuitFailure

proc main() =
  var partials: seq[string]
  var input, output: string
  var values = newJObject()

  let args = commandLineParams()
  var i = 0
  while i < args.len:
    let arg = args[i]
    if arg in ["--partials", "--template", "--output", "--data"]:
      if i + 1 >= args.len:
        fail "missing the value of " & arg
      inc i
      case arg
      of "--partials": partials.add args[i]
      of "--template": input = args[i]
      of "--output": output = args[i]
      else:
        let data = try: parseFile(args[i])
                   except CatchableError as e: fail args[i] & ": " & e.msg
        if data.kind != JObject:
          fail args[i] & ": not a JSON object"
        for key, value in data:
          values[key] = value
    elif arg.startsWith("--"):
      fail "unknown option '" & arg & "'"
    elif '=' in arg:
      let at = arg.find('=')
      values[arg[0 ..< at]] = %arg[at + 1 .. ^1]
    else:
      fail "expected key=value, got '" & arg & "'"
    inc i
  if partials.len == 0 or input.len == 0 or output.len == 0:
    fail "usage: www --partials <dir>... --template <page.html> --output <file> [--data <file.json>]... [key=value]..."

  let context = newContext(searchDirs = partials)
  for key, value in values:
    context[key] = value
  let page = try: render(readFile(input), context)
             except CatchableError as e: fail input & ": " & e.msg

  if not fileExists(output) or readFile(output) != page:
    createDir(output.parentDir)
    writeFile(output, page)

main()
