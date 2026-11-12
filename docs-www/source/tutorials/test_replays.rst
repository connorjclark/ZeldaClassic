Testing your quest with replays
===============================

When you test your quest from the editor, you can record what you play as a *replay*. A replay can be watched again later, but more usefully, it can *check* your quest: after you change something, ZQuest Classic plays the replay back by itself and tells you whether the quest still plays, and looks, the same as when you recorded it. A handful of short replays covering your dungeons, bosses and puzzles turns "did that change break something?" into a button press.

This page covers the Test Quest dialog, recording and checking replays, what to do when one fails, and sharing a replay with someone else. At the end are some ideas for how to use them.

The Test Quest dialog
---------------------

Open it with ``Quest > Test``, or ``Ctrl+T`` (``Cmd+T`` on macOS).

* **Start DMap**, **Start Screen** and **Return Square** choose where the hero appears. They start out as the screen you're editing.
* **Init Data** chooses what the hero starts with. ``<default>`` is the quest's own init data (``Quest > Init Data``). **Edit...** opens the Test Init Data dialog, where you can create named sets of items, counters and level items to test with (like "3 hearts, wooden sword" or "end-game gear") without touching the quest's own init data.
* **Test** opens the player.
* **Replays** opens the Test Replays dialog (see below).
* **Auto-Save** saves the quest before testing, so the player plays what you see in the editor.
* **Auto-Restart** restarts the player whenever you save the quest.
* **Record** records the test session as a replay.

Recording a replay
------------------

Check **Record**, click **Test**, and play. The replay is saved when you quit the player. That's all there is to it.

A replay is mostly a record of your inputs: which buttons were pressed on which frame. Test replays also record where the hero is and a fingerprint of every frame drawn, so that checking them later notices changes to what's on screen, not just to where the hero goes. They also keep a picture of each frame that looks different from the one before, which is what lets the editor show you exactly what changed when a check fails.

Replays are kept in the ``replays/test`` folder, in a folder named after the quest's file. Renaming the quest file leaves its replays behind; move the folder too if you rename it.

.. tip::
   Keep recordings short and focused: one room, one boss phase, one puzzle. A short replay is quicker to check, easier to understand when it fails, and survives changes elsewhere in the quest far better than one long playthrough.

The Test Replays dialog
-----------------------

Click **Replays** in the Test Quest dialog. It lists the replays of the current quest, newest first. Each row shows the result of the last check (if any), the replay's name, when it was recorded, how long it is, where it starts, and which test init data it used.

To the right of the list, the highlighted replay plays back on a loop, from the pictures saved with it. **Pause** stops it on the current frame.

You can select more than one replay with ``Shift+click`` or ``Ctrl+click`` (``Cmd+click`` on macOS), or all of them with ``Ctrl+A``. Play, Assert, Update and Export Bundle work on all the selected replays.

* **Play** watches the replays in the player. You can stop a replay partway (from the player's Replay menu) and keep playing from there yourself.
* **Assert** checks the replays: it plays them in the background, as fast as possible, and compares everything against the recording. Progress and results appear at the start of each row:

  * ``[passed]``: the quest still plays and looks the same.
  * ``[assert failed (frame 121)]``: something is different, starting at that frame. See `When a replay fails`_.
  * ``[assert stopped early]``, ``[crashed]``, ``[not checked]``: the check didn't finish. The text under the list says more.

* **Update** accepts changes to how the quest *looks* (new tiles, a palette change, a moved decoration) by saving them into the replays, so the next Assert checks against the new look. If the quest now *plays* differently (the hero ends up somewhere else, a screen changes at a different time), Update fails and leaves the replay as it was; record a new one instead. Updating a replay also saves pictures of its frames, if it was recorded before replays had them.
* **Stop** stops the replays that are running.
* **Details** explains why the highlighted replay failed its last Assert.
* **Rename** gives the highlighted replay a name, so you can tell "Level 3 boss" from "2026-11-11 17:57".
* **Delete** deletes the selected replays.
* **Copy Path** copies the selected replays' file paths.
* **Export Bundle** saves a zip to share. See `Sharing a replay`_.
* **Open Folder** opens the folder this quest's replays are kept in (not in the web version).

When a replay fails
-------------------

Select the failed replay and click **Details**. The dialog says when and where the quest first differed from the recording, for example *"Assert failed at 0:02 (frame 121), on DMap 1 Scr 04"*, and what happened:

* **The screen looks different, though the game played the same up to here.** Something changed in how the quest looks. If you meant it (new tiles, a palette tweak), click **Update** in the Test Replays dialog to accept it.
* **The first difference: when recorded, the hero moved to (120, 159); now, the hero moved to (120, 158).** The quest plays differently. The difference might be the hero's position, a different screen loading, a sound or an item. If you meant it, record the replay again; if you didn't, you've found a bug.

Above the explanation are two pictures of the frame it went wrong on: **When recorded** and **Now**. Check **Highlight the ... pixels that differ** to mark every pixel that changed in magenta, with everything else darkened.

**Full Screen** shows one picture at a time, as large as fits in the editor's window. Press **Switch** (or ``S``) to flip between "When recorded" and "Now": flipping back and forth is the quickest way to spot a small change. ``H`` toggles the highlighting.

**Replay to Failure** opens the player, plays the replay up to just before it went wrong, and pauses there. From there you can look around (with cheats, the debugger and so on) to see what's going on. The player then asks what to do next:

* **Just Play** lets you keep playing from that point. The replay is left as it was.
* **Record** records your play from that point on, and when you quit, it replaces the rest of the replay. This keeps everything before the change, so you only need to replay the part that's different.

Sharing a replay
----------------

**Export Bundle** (in the Test Replays dialog, or in the Details dialog of a failed replay) saves a zip containing:

* the selected replays
* the quest file itself
* what was saved with each replay: the result of its last check, screenshots of where it failed, and the pictures of its frames
* a README explaining how to play them

Anyone with the same version of ZQuest Classic can unzip it and play or Assert a replay from the player's Replay menu, and see exactly what you saw. This is the best way to report a bug: attach the bundle to a bug report or a message instead of writing "walk to screen 3F and swing at the left wall".

.. warning::
   The bundle includes your quest file, and anyone you share it with can open it in the editor. Only share it with people you'd share the quest with.

Bundles of long replays can be large. Some chat apps limit how large an upload can be (Discord without Nitro takes up to 10 MB); the editor warns you when a bundle is bigger than that, and a file sharing site can take it instead.

Ideas for using test replays
----------------------------

Catching breakage
^^^^^^^^^^^^^^^^^

* **Pre-release check**: record a short run of each dungeon, boss and puzzle once, then select them all and click **Assert** before publishing an update. Anything not ``[passed]`` is a place the quest now plays differently.
* **Script refactors**: after changing a global or shared script, Assert everything rather than re-walking every screen that uses it.
* **Engine upgrades**: Assert the whole set after moving to a new version of ZQuest Classic. A failure is either a compatibility bug (please report it, with a bundle!) or a behaviour change you need to know about.

Fixing bugs
^^^^^^^^^^^

* **Capture, then fix**: record the bug happening, change the quest, then **Play** the replay to watch the same inputs against your fix. Once it's fixed, record the area again, and it becomes a permanent check that the bug stays fixed.
* **Bug reports**: use **Export Bundle** to send a deterministic reproduction to the person who can fix it, whether that's the ZQuest Classic developers or the author of a script you're using.

Saving time while building
^^^^^^^^^^^^^^^^^^^^^^^^^^

* **Reaching a late moment**: when you're iterating on something at the end of a long cutscene or boss fight (a string, a palette, the music), **Play** a replay to get there instead of playing through it again by hand. Stop the replay when it gets there, and keep playing.
* **Intentional changes**: after retiling a room or changing a palette, **Update** rewrites what the replays expect, without replaying them by hand.
* **Partial re-recording**: when a change means the end of a replay plays differently, use **Replay to Failure** and **Record** to redo only that part.

Using test init data
^^^^^^^^^^^^^^^^^^^^

* **Minimum-equipment proofs**: a recording made with "3 hearts, no sword upgrade" shows the boss is still beatable with that, and Assert tells you if a later change makes it impossible.
* **Sequence-break guards**: record an attempt to get past a gate without the item it requires, so Assert flags it if a later change opens the gate.

Limits
------

* A replay is a record of inputs. If you change something the hero walks through *before* the part you care about (moving a wall, adding an enemy, changing a door), the rest of the replay goes differently, and it needs to be recorded again rather than updated. Short, focused recordings make this much less painful.
* **Update** only accepts changes to how the quest looks. Anything that changes where the hero goes, or when screens change, needs a new recording (or **Replay to Failure** and **Record**).
* Results, failures and bundles are specific to the version of ZQuest Classic that made them. Check against the same version you recorded with, unless you're deliberately testing an upgrade.
