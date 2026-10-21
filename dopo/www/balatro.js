/**
 * The Balatro patcher: opens the player's Balatro.exe or Balatro.love,
 * applies the chosen dopo_* patchs with dopoPatchs (patchs.nim built for
 * JS) and packs a new .love. A patch is one file per language it touches,
 * dopo_<name>.lua for the Lua sources and dopo_<name>.fs for the shaders.
 * JSZip and dopoPatchs are globals, loaded by the page as classic scripts.
 */

/** the patchs are line based: they only fit this release */
export const GAME_VERSION = '1.0.1o-FULL';

const call = (request) => {
    const response = JSON.parse(dopoPatchs(JSON.stringify(request)));
    if (response.error) throw new Error(response.error);
    return response;
};

const fetchOk = async (url) => {
    const response = await fetch(url);
    if (!response.ok) throw new Error(`${url}: ${response.status}`);
    return response;
};

/**
 * The patchs of a list, each {name, group, files: [{name, text}], title,
 * description}; title and description come from the line comment (-- or
 * //) that opens its first file.
 */
async function fetchPatchs(base, list) {
    return Promise.all(list.map(async ({name, group, files}) => {
        files = await Promise.all(files.map(async (file) => ({
            name: file,
            text: await (await fetchOk(new URL(`patchs/${file}`, base))).text(),
        })));
        const header = [];
        for (const line of files[0].text.split('\n')) {
            const comment = line.match(/^(?:--|\/\/)(?!!)\s?(.*)$/);
            if (!comment) break;
            header.push(comment[1]);
        }
        const [title = name, ...rest] = header;
        return {name, group, files, title, description: rest.join(' ').trim()};
    }));
}

/** the patchs of base/patchs.json's groups, in order */
export async function loadPatchs(base) {
    const groups = await (await fetchOk(new URL('patchs.json', base))).json();
    return fetchPatchs(base, groups.flatMap(({group, patchs}) => patchs.map((patch) => ({...patch, group}))));
}

/** the patchs of base/web.json, which the game needs to run in a browser */
export async function loadWebPatchs(base) {
    return fetchPatchs(base, await (await fetchOk(new URL('web.json', base))).json());
}

/* the game the play page runs: in love.js's own store, where its player
 * looks first, under the name the play page asks it for */
const STORE = 'EM_PRELOAD_CACHE';
const PACKAGE = 'Balatro.love';

function packages(mode, use) {
    return new Promise((resolve, reject) => {
        const open = indexedDB.open(STORE, 1);
        open.onupgradeneeded = () => open.result.createObjectStore('PACKAGES');
        open.onerror = () => reject(open.error);
        open.onsuccess = () => {
            const db = open.result;
            const request = use(db.transaction('PACKAGES', mode).objectStore('PACKAGES'));
            request.onsuccess = () => { db.close(); resolve(request.result); };
            request.onerror = () => { db.close(); reject(request.error); };
        };
    });
}

/** whether a game is on this device for the play page */
export async function hasPlayable() {
    return Boolean(await packages('readonly', (store) => store.getKey(PACKAGE)).catch(() => undefined));
}

/** leaves game, with what it needs to run in a browser, for the play page */
export async function storePlayable(game, webPatchs, progress) {
    const blob = await buildLove(game, webPatchs, progress);
    const data = new Uint8Array(await blob.arrayBuffer());
    await packages('readwrite', (store) => store.put(data, PACKAGE));
}

/** the patch names a defaults file lists, one a line ('#' starts a comment) */
export async function loadSelection(url) {
    const text = await (await fetchOk(url)).text();
    return text.split(/\r?\n/).map((line) => line.trim()).filter((line) => line && !line.startsWith('#'));
}

const filesOf = (patchs) => patchs.flatMap((patch) => patch.files);

/** name -> names of the patchs it can not be applied with */
export function findConflicts(patchs) {
    const conflicts = new Map(patchs.map((patch) => [patch.name, new Set()]));
    for (let i = 0; i < patchs.length; i++) {
        call({patches: patchs[i].files});
        for (let j = i + 1; j < patchs.length; j++) {
            try {
                call({patches: filesOf([patchs[i], patchs[j]])});
            } catch {
                conflicts.get(patchs[i].name).add(patchs[j].name);
                conflicts.get(patchs[j].name).add(patchs[i].name);
            }
        }
    }
    return conflicts;
}

/**
 * A fused Balatro.exe is love.exe with the game's zip appended, its offsets
 * relative to where the zip starts: the zip is cut out by its end record.
 */
function zipStart(bytes) {
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const lowest = Math.max(0, bytes.length - 22 - 0xffff);
    for (let at = bytes.length - 22; at >= lowest; at--) {
        if (view.getUint32(at, true) !== 0x06054b50) continue;
        const size = view.getUint32(at + 12, true);
        const offset = view.getUint32(at + 16, true);
        const start = at - size - offset;
        if (start >= 0) return start;
    }
    throw new Error('this is not a Balatro.exe or Balatro.love');
}

/** {bytes, zip, version} of a Balatro.exe or Balatro.love's bytes */
export async function openGame(buffer) {
    const all = new Uint8Array(buffer);
    const bytes = all.subarray(zipStart(all));
    const zip = await JSZip.loadAsync(bytes);
    const versionFile = zip.file('version.jkr');
    if (!versionFile || !zip.file('main.lua')) {
        throw new Error('no Balatro in this file');
    }
    const version = (await versionFile.async('string')).split(/\r?\n/)[0].trim();
    return {bytes, zip, version};
}

/**
 * game's zip with the chosen patchs applied, as a .love Blob; progress gets
 * the packing's percentage
 */
export async function buildLove(game, patchs, progress = () => {}) {
    if (game.version !== GAME_VERSION) {
        throw new Error(`these patchs are for Balatro ${GAME_VERSION}, this one is ${game.version}`);
    }
    const patches = filesOf(patchs);
    const {targets} = call({patches});
    const paths = Object.keys(game.zip.files).filter((path) => !game.zip.files[path].dir);
    const files = {};
    for (const target of targets) {
        const file = game.zip.file(target);
        if (file) files[target] = await file.async('string');
    }
    const patched = call({patches, paths, files});

    // a fresh copy of the game's zip, so the files left alone keep their
    // compressed data instead of being packed again
    const zip = await JSZip.loadAsync(game.bytes);
    for (const path of patched.deletes) zip.remove(path);
    for (const [path, text] of Object.entries(patched.files)) zip.file(path, text);
    return zip.generateAsync(
        {type: 'blob', mimeType: 'application/zip', compression: 'DEFLATE'},
        (meta) => progress(meta.percent));
}
