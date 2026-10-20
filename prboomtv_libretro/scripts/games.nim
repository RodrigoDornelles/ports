## games: the web build's game pages, from a published games.json.
##
## usage: games --games <games.json> --url <its url> --template <game.html>
##              --output <dir>
##
## Every game of --games ({"title", "url", "banner", "demo", "y90", "modes"},
## url and banner relative to the games.json, published at --url) gets
## <output>/<id>.html, --template with @GAME_ID@, @GAME_TITLE@,
## @GAME_CONTENT@ (the playlist's URL) and @GAME_BANNER@ (the banner's URL)
## filled in; the id is the playlist's name (doom1.m3u: doom1.html). The
## index's list goes to <output>/games.json: id, title, banner URL and the
## flags. Outputs are only rewritten on change.

import std/[json, os, parseopt, strutils]

proc fail(message: string) {.noreturn.} =
  stderr.writeLine "games: ", message
  quit QuitFailure

proc writeChanged(path, text: string) =
  if not fileExists(path) or readFile(path) != text:
    writeFile(path, text)

## the template puts every value in an HTML attribute or text, never in
## script source: the page reads the title back from its <meta>
proc escapeHtml(text: string): string =
  text.multiReplace({"&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;"})

## the playlist's name without its extension: a page and a URL path segment
proc gameId(url: string): string =
  let (dir, name, ext) = splitFile(url)
  if dir != "" or ext == "" or name == "" or
      not name.allCharsInSet({'A'..'Z', 'a'..'z', '0'..'9', '_', '-'}):
    fail "url '" & url & "' is not a playlist file name next to games.json"
  name

const usage = "usage: games --games <games.json> --url <its url> --template <game.html> --output <dir>"

proc main() =
  var gamesPath, url, templatePath, output: string
  # with a longNoVal, the other options take the next argument as value
  for kind, key, value in getopt(longNoVal = @["help"]):
    if kind != cmdLongOption: fail "unexpected argument '" & key & "'"
    case key
    of "games": gamesPath = value
    of "url": url = value
    of "template": templatePath = value
    of "output": output = value
    of "help":
      echo usage
      quit QuitSuccess
    else: fail "unknown option --" & key
  if gamesPath == "" or url == "" or templatePath == "" or output == "":
    fail usage
  # the folder of the games.json, which its paths are relative to
  let base = url[0 .. url.rfind('/')]

  let games = try: parseFile(gamesPath)
              except CatchableError as e: fail gamesPath & ": " & e.msg
  if games.kind != JArray: fail gamesPath & ": not a list of games"
  let page = readFile(templatePath)
  createDir(output)

  var index = newJArray()
  var seen: seq[string]
  for i, game in games.elems:
    if game.kind != JObject or game{"title"}.getStr == "" or game{"url"}.getStr == "":
      fail gamesPath & ": game " & $i & " needs a title and a url"
    let id = gameId(game["url"].getStr)
    if id in seen: fail gamesPath & ": two games make " & id & ".html"
    seen.add id
    let title = game["title"].getStr
    let banner = if game{"banner"}.getStr == "": "" else: base & game["banner"].getStr

    writeChanged(output / id & ".html", page.multiReplace({
      "@GAME_ID@": id,
      "@GAME_TITLE@": escapeHtml(title),
      "@GAME_CONTENT@": escapeHtml(base & game["url"].getStr),
      "@GAME_BANNER@": escapeHtml(banner)}))

    var modes = newJArray()
    for mode in game{"modes"}.getElems:
      modes.add newJString(mode.getStr)
    index.add %*{"id": id, "title": title, "banner": banner,
                 "demo": game{"demo"}.getBool, "y90": game{"y90"}.getBool,
                 "modes": modes}

  var text = "[\n"
  for i, entry in index.elems:
    text.add "  " & $entry & (if i < index.len - 1: ",\n" else: "\n")
  text.add "]\n"
  writeChanged(output / "games.json", text)

main()
