## ImGuiColorTextEdit

Fork of: https://github.com/BalazsJako/ImGuiColorTextEdit (Author: BalazsJako)

Modifications (search "local edit" to find all code changes):

* Changed the existing mBreakpoints to mLineHighlights, and made
  mBreakpoints draw red circles when line gutter is clicked.
* Made breakpoints able to be disabled/enabled.
* Added mExecutableLines and PaletteIndex::LineNumberDimmed to represent
  lines that can't have a breakpoint.
* Improved EnsureCursorVisible to scroll the line to be roughly in the middle
  of the screen.
* Fix Ctrl/Cmd on Mac for newer Imgui.
* Add SetHoverCallback to signal the word under the mouse cursor.
