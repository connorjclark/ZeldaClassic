// Shares single files in /local (the persisted folder) with other tabs.
//
// FS.syncfs makes one side match the other entirely: saving also deletes whatever the store
// has that this tab doesn't, and writes over whatever differs. That loses files another tab
// wrote since this one loaded, like the replays and results that a player tab opened by the
// editor writes for it. These touch only the paths they're given.

import { getAttachedDirHandle } from "./settings.js";

const LOCAL = '/local';

// One at a time, in order, so a save of everything doesn't run in the middle of these.
let pending = Promise.resolve();
export function fsEnqueue(fn) {
  const result = pending.then(fn);
  pending = result.catch(e => console.error(e));
  return result;
}

function toLocalPath(path) {
  if (!path.startsWith('/')) path = '/' + path;
  path = PATH.normalize(path);
  return path === LOCAL || path.startsWith(LOCAL + '/') ? path : null;
}

function isIdb() {
  return FS.lookupPath(LOCAL).node.mount.type === IDBFS;
}

function localStat(path) {
  try {
    return FS.stat(path);
  } catch {
    return null;
  }
}

// The paths and the folders they're in, folders first, leaving out what isn't in /local or
// doesn't exist.
function withParents(paths) {
  const all = new Set();
  for (let path of paths) {
    path = toLocalPath(path);
    if (!path || !localStat(path)) continue;
    for (; path !== LOCAL; path = PATH.dirname(path)) all.add(path);
  }
  return [...all].sort();
}

function idbTransaction(mode, run) {
  return new Promise((resolve, reject) => {
    IDBFS.getDB(LOCAL, (err, db) => {
      if (err) return reject(err);

      const transaction = db.transaction([IDBFS.DB_STORE_NAME], mode);
      transaction.oncomplete = () => resolve();
      transaction.onerror = transaction.onabort = () => reject(transaction.error);
      run(transaction.objectStore(IDBFS.DB_STORE_NAME));
    });
  });
}

async function attachedDir(relPath, create) {
  let handle = getAttachedDirHandle();
  for (const name of relPath.split('/').filter(Boolean)) {
    handle = await handle.getDirectoryHandle(name, { create });
  }
  return handle;
}

function toRelPath(path) {
  return path.substring(LOCAL.length + 1);
}

// Saves these files, and the folders they're in.
export function fsPersist(paths) {
  paths = [...paths];
  return fsEnqueue(async () => {
    const all = withParents(paths);
    if (!all.length) return;

    if (isIdb()) {
      await idbTransaction('readwrite', (store) => {
        for (const path of all) {
          IDBFS.loadLocalEntry(path, (err, entry) => {
            if (!err) IDBFS.storeRemoteEntry(store, path, entry, () => { });
          });
        }
      });
      return;
    }

    for (const path of all) {
      const stat = FS.stat(path);
      if (FS.isDir(stat.mode)) {
        await attachedDir(toRelPath(path), true);
      } else if (FS.isFile(stat.mode)) {
        const dir = await attachedDir(toRelPath(PATH.dirname(path)), true);
        const handle = await dir.getFileHandle(PATH.basename(path), { create: true });
        const writable = await handle.createWritable();
        await writable.write(FS.readFile(path));
        await writable.close();
      }
    }
  });
}

// Deletes these files from what's saved. They are expected to be gone from this tab already.
export function fsRemove(paths) {
  paths = paths.map(toLocalPath).filter(Boolean);
  return fsEnqueue(async () => {
    if (!paths.length) return;

    if (isIdb()) {
      await idbTransaction('readwrite', (store) => {
        for (const path of paths) IDBFS.removeRemoteEntry(store, path, () => { });
      });
      return;
    }

    for (const path of paths) {
      try {
        const dir = await attachedDir(toRelPath(PATH.dirname(path)), false);
        await dir.removeEntry(PATH.basename(path));
      } catch {
        // Already gone.
      }
    }
  });
}

function storeLocalFile(path, contents, time) {
  FS.mkdirTree(PATH.dirname(path));
  FS.writeFile(path, contents);
  FS.utime(path, time, time);
}

async function pullAttachedDir(dirHandle, path) {
  for await (const [name, handle] of dirHandle.entries()) {
    const childPath = PATH.join2(path, name);
    if (handle.kind === 'directory') {
      FS.mkdirTree(childPath);
      await pullAttachedDir(handle, childPath);
      continue;
    }

    const file = await handle.getFile();
    const stat = localStat(childPath);
    if (!stat || file.lastModified > stat.mtime.getTime()) {
      storeLocalFile(childPath, new Uint8Array(await file.arrayBuffer()), file.lastModified);
    }
  }
}

// Loads whatever is saved in this folder that this tab doesn't have, or has an older copy of.
// Nothing in this tab is deleted.
export function fsPull(dir) {
  dir = toLocalPath(dir);
  return fsEnqueue(async () => {
    if (!dir) return;

    if (isIdb()) {
      await idbTransaction('readonly', (store) => {
        const found = [];
        const request = store.index('timestamp').openKeyCursor();
        request.onsuccess = () => {
          const cursor = request.result;
          if (cursor) {
            const path = cursor.primaryKey;
            if (path === dir || path.startsWith(dir + '/')) found.push([path, cursor.key]);
            cursor.continue();
            return;
          }

          // Folders before the files in them.
          found.sort((a, b) => a[0] < b[0] ? -1 : 1);
          for (const [path, time] of found) {
            const stat = localStat(path);
            if (stat && time.getTime() <= stat.mtime.getTime()) continue;

            IDBFS.loadRemoteEntry(store, path, (err, entry) => {
              if (err) return;
              FS.mkdirTree(PATH.dirname(path));
              IDBFS.storeLocalEntry(path, entry, () => { });
            });
          }
        };
      });
      return;
    }

    let dirHandle;
    try {
      dirHandle = await attachedDir(toRelPath(dir), false);
    } catch {
      // Nothing has been saved there yet.
      return;
    }
    await pullAttachedDir(dirHandle, dir);
  });
}
