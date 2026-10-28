## games: the web build's game pages, from a published games.json.
##
## usage: games --games <games.json> --url <its url> --template <game.html>
##              --manifest <game.webmanifest> --output <dir>
##
## Every game of --games ({"title", "url", "banner", "demo", "y90", "modes"},
## url and banner relative to the games.json, published at --url) gets
## <output>/<id>.html, the mustache --template with {{id}}, {{title}},
## {{content}} (the playlist's URL) and {{banner}} (the banner's URL); the id
## is the playlist's name (doom1.m3u: doom1.html). Each page also gets
## <output>/<id>.webmanifest from --manifest, which makes the game an app of
## its own to install: its scope is the page alone, so every game installs
## apart from the others in the same folder; the manifest has the values as
## JSON strings too, {{{title_json}}}. The index's list goes to
## <output>/games.json: id, title, playlist and banner URLs and the flags. Outputs are
## only rewritten on change.

import std/[json, os, parseopt, strutils]
import mustache

proc fail(message: string) {.noreturn.} =
  stderr.writeLine "games: ", message
  quit QuitFailure

proc writeChanged(path, text: string) =
  if not fileExists(path) or readFile(path) != text:
    writeFile(path, text)


## the playlist's name without its extension: a page and a URL path segment
proc gameId(url: string): string =
  let (dir, name, ext) = splitFile(url)
  if dir != "" or ext == "" or name == "" or
      not name.allCharsInSet({'A'..'Z', 'a'..'z', '0'..'9', '_', '-'}):
    fail "url '" & url & "' is not a playlist file name next to games.json"
  name

const usage = "usage: games --games <games.json> --url <its url> --template <game.html> --manifest <game.webmanifest> --output <dir>"

proc main() =
  var gamesPath, url, templatePath, manifestPath, output: string
  # with a longNoVal, the other options take the next argument as value
  for kind, key, value in getopt(longNoVal = @["help"]):
    if kind != cmdLongOption: fail "unexpected argument '" & key & "'"
    case key
    of "games": gamesPath = value
    of "url": url = value
    of "template": templatePath = value
    of "manifest": manifestPath = value
    of "output": output = value
    of "help":
      echo usage
      quit QuitSuccess
    else: fail "unknown option --" & key
  if gamesPath == "" or url == "" or templatePath == "" or manifestPath == "" or output == "":
    fail usage
  # the folder of the games.json, which its paths are relative to
  let base = url[0 .. url.rfind('/')]

  let games = try: parseFile(gamesPath)
              except CatchableError as e: fail gamesPath & ": " & e.msg
  if games.kind != JArray: fail gamesPath & ": not a list of games"
  let page = readFile(templatePath)
  let manifest = readFile(manifestPath)
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

    # the page puts every value in an HTML attribute or text, never in
    # script source: it reads the title back from its <meta>
    let description = "Play " & title & " with PrBoomTV, alone or with friends."
    let values = newContext()
    values["id"] = id
    values["title"] = title
    values["content"] = base & game["url"].getStr
    values["banner"] = banner
    values["title_json"] = $(%title)
    values["description_json"] = $(%description)
    values["banner_json"] = $(%banner)
    writeChanged(output / id & ".html", render(page, values))
    writeChanged(output / id & ".webmanifest", render(manifest, values))

    var modes = newJArray()
    for mode in game{"modes"}.getElems:
      modes.add newJString(mode.getStr)
    index.add %*{"id": id, "title": title, "content": base & game["url"].getStr, "banner": banner,
                 "demo": game{"demo"}.getBool, "y90": game{"y90"}.getBool,
                 "modes": modes}

  var text = "[\n"
  for i, entry in index.elems:
    text.add "  " & $entry & (if i < index.len - 1: ",\n" else: "\n")
  text.add "]\n"
  writeChanged(output / "games.json", text)

main()
