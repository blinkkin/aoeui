ifdef(`ASDFG',`define(`AOEUI',`asdfg')define(`cmd',`$2')',`define(`AOEUI',`aoeui')define(`cmd',`$1')')dnl
Welcome to AOEUI 1.5g!  Here are some clues to help you use the editor.

- The up/down/left/right "arrow" keys, page up/down keys, and Delete key
  all work fine.  You can use AOEUI as a simple notepad if you like.
- To insert text into a document, just type it.
- In this documentation, the notation ^A means the command A, which you
  access by holding down Control or Alt while hitting A, or by pressing
  Escape and then A.  All of these modifiers mean the same thing in AOEUI.
- The space bar, when used as a command (^Sp), is a prefix that distinguishes
  command variants and numeric arguments.

Command summary:

^Sp?   display this help again
   ^Q  pause the editor and return to the shell; return with "fg"
^Sp^Q  save all files and quit
^Sp^\  quit immediately without saving
   ^cmd(U,Z)  undo                        ^Sp^cmd(U,Z)  redo
   ^cmd(K,W)  save all files              ^Sp^cmd(K,W)  save one file

   ^cmd(H,G)  backward                       ^cmd(T,H)  forward
^Sp^cmd(H,G)  up                          ^Sp^cmd(T,H)  down
   ^cmd(N,K)  previous word                  ^cmd(S,L)  next word
^Sp^cmd(N,K)  previous sentence           ^Sp^cmd(S,L)  next sentence
   ^cmd(G,T)  previous beginning of line     ^cmd(C,Y)  next end of line
^Sp^cmd(G,T)  paragraph start             ^Sp^cmd(C,Y)  paragraph end
   ^cmd(R,O)  previous page                  ^cmd(L,P)  next page
^Sp^cmd(R,O)  go to beginning             ^Sp^cmd(L,P)  go to end
   ^]  go to nearest ([{bracket}]), or to corresponding bracket
^Sp n ^cmd(Z,N)  go to line number n

   ^cmd(V,U)  begin a selection if none, forgets selection otherwise
^Sp^cmd(V,U)  go to opposite end of selection, or select whole line if none
   ^cmd(D,X)  cut, replacing clip buffer
         (typing new text at the start of the selection also cuts)
^Sp^cmd(D,X)  with selection: cut, adding to clip buffer
^Sp^cmd(D,X)  without selection: select all white space surrounding cursor
   ^cmd(F,C)  copy, replacing clip buffer
^Sp^cmd(F,C)  copy, adding to clip buffer
   ^cmd(B,V)  exchange clip buffer with selection, if any; else paste

   ^J  insert new line with automatic alignment (^Return may also work)
   Tab and ^I attempt tab completion if no selection is present
^SpTab align current line (^Sp^I also works)
   ^^  insert next character as raw or control character
^Sp n ^^  insert Unicode character n -- use leading 0x for hexadecimal

   ^_  incremental search mode (^-, ^/, and ^A may also work)
^Sp^_  incremental regular expression search mode with POSIX regexps.
         In search mode, use ^cmd(H,G) and ^cmd(T,H) to move from one
         instance of the search target to another and any other command,
	 or Return, to resume editing.

   ^cmd(X,E)  open file named by selection in new window
         insert current path as selection if none
^Sp^cmd(X,E)  rename current text with path in selection
   ^cmd(W,F)  display another open view in this window
^Sp^cmd(W,F)  close this window and its text
   ^cmd(Y,D)  split current window horizontally
^Sp^cmd(Y,D)  split current window vertically
   ^cmd(P,S)  switch to another window
^Sp^cmd(P,S)  close current window

^Sp^cmd(O,B)  begin recording default macro (end with ^cmd(O,B))
   ^cmd(O,B)  run default macro, or end macro/function key recording
^SpF1-F12  begin recording function key macro (end with ^cmd(O,B))
   F1-F12  execute function key macro

   ^cmd(E,R)  run shell command in selection with clip buffer as input,
         or open new shell interaction window if no selection
^Sp^cmd(E,R)  cancel all pending background commands

Parting words:
- Many commands support a repeat count; ^Sp9^cmd(T,H) advances nine characters.
- AOEUI has bookmarks, registers, tags, folding, and other features.
  Read the manual page for the full story and lots of useful tips.
- Send me a note at pmk@google.com and say hi!
