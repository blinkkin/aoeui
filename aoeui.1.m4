ifdef(`ASDFG',`define(`AOEUI',`asdfg')define(`cmd',`$2')',`define(`AOEUI',`aoeui')define(`cmd',`$1')')dnl
.\" Man page for AOEUI
.\"
.\" Copyright 2007 Peter Klausler
.\" Released under GPLv2.
.TH AOEUI 1 "June 27, 2007"
.LO 1
.SH NAME
AOEUI \- a lightweight visual editor optimized for the cmd(Dvorak,QWERTY) keyboard
.SH SYNOPSIS
.B AOEUI
[
.B -t
.I "tab stop"
]
.RI [ file... ]
.SH DESCRIPTION
.B AOEUI
is an interactive display text editor optimized for users of the
cmd(Dvorak,QWERTY) keyboard layout.
.P
When run with no file name arguments,
.B AOEUI
displays a short command introduction and summary.
.P
.B AOEUI
can browse very large read-only files with quick start-up,
since original texts are memory-mapped from files and not
duplicated in memory until they are about to be modified.
.SH "INTENTIONALLY MISSING FEATURES"
.B AOEUI
has no embedded extension language, since it is trivial to
pass regions of text from the editor to any program or script
that can read standard input and write standard output.
The shell,
.BI sed (1),
.BI awk (1),
.BI python (1),
and
.BI perl (1)
are all usable for such scripting.
Further, since
.B AOEUI
ships will full sources and the rights to modify it,
users can customize it directly.
.P
The editor has no syntax highlighting modes, apart from
subtle color cues that help match up parentheses, brackets,
and braces.
.P
.B AOEUI
has no mail or news reader, IRC client, or artificial intelligence
psychologist mode.
.P
There is no X window system interface; that's what
.BI xterm (1)
and
.BI gnome-terminal (1)
are for.
.SH BASICS
A
.I text
is a sequence of characters to be viewed or edited, such as a file.
A
.I view
comprises all or part of a text.
A text in the editor has at least one view, and possibly more.
.P
A
.I window
is a rectilinear portion of the display, and is always associated
with a single view, a contiguous portion of whose text is rendered
in the window.  Not every view has a window.
.P
Each view has a
.I cursor
and possibly a
.IR selection ,
which has the cursor at one end and the
.I mark
at the other.
The view's window, if any, always renders part of the text containing
the view's cursor.
.P
The
.I clip buffer
is not visible in any window.
It receives snippets of data that have been cut or copied out of
texts, so that they may be moved or copied elsewhere.
It also supplies the standard input to a background command
launched with
.B ^cmd(E,R)
(below).
There is one clip buffer shared by all views.
.SH "COLOR CUES"
AOEUI uses colors to convey information without cluttering the
display with status lines or borders between windows.
.P
AOEUI uses distinct background colors to distinguish tiled windows.
The active window is always presented in the terminal's default
color scheme.
Color is also used to highlight the current selection (in cyan)
and folded regions (in red).
.P
Needless tabs and spaces are marked in violet.  These include
any tabs or spaces before the end of a line, as well as any
spaces followed by a tab or multiple spaces that could be
replaced by a tab.
.P
Bracketing characters are presented in alternating colors so that
matching parentheses, brackets, and braces are colored identically.
.P
A red cursor signifies a read-only text, whereas a green cursor
indicates a dirty text (meaning one that needs saving, not one
unfit for young persons).
.SH COMMANDS
.P
Commands are denoted here by
.B ^key
to signify the use of Control, Alt, or a leading Escape key.
They all mean the same thing.
.P
.B Variant
commands always begin with
.BR ^Space ,
or its synonym,
.BR ^@ .
A few commands take a numeric argument, which is specified by
.B ^Space
followed by a decimal or hexadecimal number, the latter
using C language syntax (0xdeadbeef).
.P
Many commands are sensitive to the presence or absence of a
.BR selection .
.SH LEAVING
.TP
.B ^Space^\e
aborts the editor, leaving no original file modified since the
last time
.B ^cmd(K,W)
was used.
.TP
.B ^cmd(Q,Q)
suspends the editor and returns the terminal to the shell that
invoked it.
Use the shell's foreground command, probably
.BR fg ,
to resume editing.
.TP
.B ^Space^cmd(Q,Q)
saves all modified texts and terminates the editor.
.SH NAVIGATION
The "backward and forward by unit" commands treat a numeric argument,
if any, as a repeat count.
.TP
.B ^cmd(H,G)
moves the cursor backward by characters.
.TP
.B ^cmd(T,H)
moves the cursor forward by characters.
.TP
.B ^cmd(N,K)
moves the cursor backward by words.
.TP
.B ^cmd(S,L)
moves the cursor forward by words.
.TP
.B ^Space^cmd(N,K)
moves the cursor backward one sentence.
.TP
.B ^Space^cmd(S,L)
moves the cursor forward one sentence.
.TP
.B ^cmd(G,T)
moves the cursor back to the beginning of the line.
If already there, it moves back to the beginning of the previous line.
.TP
.B ^cmd(C,Y)
moves the cursor forward to the end of the line.
If already there, it moves forward to the end of the next line.
.TP
.B ^cmd(R,O)
moves the window backward by screenfulls.
.TP
.B ^cmd(L,P)
moves the window forward by screenfulls.
.TP
.B ^Space^cmd(R,O)
moves to the very beginning of the view.
.TP
.B ^Space^cmd(L,P)
moves to the very end of the view.
.TP
.B ^]
moves to the corresponding parenthesis, bracket, or brace, respecting
nesting, if the cursor sits atop such a character.
Otherwise, it moves to the nearest enclosing bracketing character.
.TP
.B ^cmd(Z,N)
recenters the window so that the line containing the cursor lies in
the middle of its portion of the display.
.TP
.B ^Space^cmd(Z,N)
causes the current window to occupy the entire display and recenters
the window.
With a numeric argument, however, it simply
moves the cursor to the indicated line in the view, with 1 being the
number of the first line.
.TP
.B ^Space=
(note that
.B =
is not a control character)
sets a bookmark on the current selection or cursor position.
A numeric argument may be used to manage multiple bookmarks.
.TP
.B ^Space-
(note that
.B -
is not a control character)
returns to a previous set bookmark, possibly identified with a
numeric argument.
.TP
.B ^Space'
(note that the single quote
.B .
is not a control character)
looks an identifier up the identifier in the
.B TAGS
file, which is automatically loaded into the editor if it is
not already present, and if the identifier is found therein,
the corresponding file name and line number are sought.
The
.B TAGS
file should be generated with the
.B ctags
utility and its
.B -x
output format.
If there is a selection, it is deleted from the view and its entire contents
will constitute the identifier to be looked up; otherwise, the identifier
that is immediately before or around the cursor is sought.
.SH SELECTION
These commands are sensitive to the presence or absence of a current selection.
.TP
.B ^cmd(V,U)
begins a new selection if non exists, setting its mark at the current cursor,
which is then typically navigated to its intended other end.
.B ^cmd(V,U)
in the presence of selection simply removes the mark.
.TP
.B ^Space^cmd(V,U)
without a selection causes the entire current line to be
selected by placing the mark at the end of the line and the cursor at
its beginning.  It is the same as the command sequence
.B ^cmd(C,Y)^cmd(V,U)^cmd(G,T)
with no selection.
With a selection present,
.B ^Space^cmd(V,U)
exchanges its cursor with its mark.
.P
Note that
.B ^Space^cmd(V,U)
with a numeric argument unconditionally unsets the mark, which can be
handy in a macro.
.TP
.B ^Space^cmd(D,X)
with no selection causes all of the contiguous white space characters
surrounding the cursor to be selected, with the cursor at the beginning so
that they can be easily replaced by retyping.
.SH UNDO
.B AOEUI
has infinite undo capabilities.
.TP
.B ^cmd(U,Z)
reverses the effects of the last command, apart from
.B ^cmd(U,Z)
itself, that modified the current text in any of its views.
.TP
.B ^Space^cmd(U,Z)
reverses the effects of the most recent undo.
After
.BR ^cmd(U,Z) ,
any
.I other
command that modifies the text will permanently commit the undo(s).
.SH MODIFICATION
In the default mode, characters typed without a command indicator
are inserted at the current cursor position.
Further, if the cursor is at the beginning of a selection, the selection is
first cut to the clip buffer, so that the new text replaces it.
.TP
.B ^^
(that's Control-Shift-6, the caret character, on most keyboards,
and ^6 will probably also work)
inserts an untypeable control character into the text.
The very next key to be pressed is either taken literally,
if it is a control character, or converted to a control character
if it is not, and inserted.
(For example, you can press
.B ^^
and then hit either Control-A or just a plain A, to get the
character code 0x01 inserted.)
.TP
.B ^Space^^
with a numeric argument, probably in hexadecimal, inserts the
specified Unicode character into the text in UTF-8 format.
.TP
.B Tab
(or
.BR ^I )
attempts to perform tab completion; if that fails, a TAB character
is inserted.
If there is a selection with the cursor at its end, the editor
tries to find an unambiguous continuation based on path names
and words in all the views.
A continuation, if found, is appended to the selection, to
facilitate opening a file with
.BR ^cmd(X,E) .
With no selection, but the cursor immediately after one or more
identifier characters, the editor searches for an unambiguous
continuation using the words in the views.
A continuation, if found, is inserted as the new selection
with the cursor at its end.
No tab completion occurs when the cursor is at the beginning
of a selection; in that case, the selection is cut and replaced
with a single TAB character.
.TP
.B ^SpaceTab
(or
.BR ^Space^I )
will align the current line to the indentation of the previous one.
With a numeric argument between 1 and 20, it will set the tab stop pitch.
.TP
.B ^J
(or
.B ^Enter
under some terminal emulators)
inserts a new line into the text with automatic indentation.
.TP
.B ^SpaceEnter
(or
.BR ^Space^M )
opens up a new line after the current one.
.TP
.B Backspace
(or more properly, its synonym
.BR ^? ),
deletes the character immediately before the cursor.
.TP
.B ^cmd(D,X)
with no selection deletes the character "under" the cursor.
When a selection exists,
.B ^cmd(D,X)
moves it into the clip buffer, discarding any previously clipped text.
.TP
.B ^Space^cmd(D,X)
with no selection will select surrounding white space, as described
earlier.
When a selection exists,
.B ^Space^cmd(D,X)
moves it into the clip buffer, putting it before any old text if the cursor
was at its beginning and appending it to the clip buffer if the cursor
was at its end.
The intent is for multiple
.B ^Space^cmd(D,X)
commands to collect data together in the same order in which
they are most likely to have been visited.
.TP
.B ^cmd(F,C)
requires a selection, which is copied into the clip buffer and
then unmarked.
.TP
.B ^Space^cmd(F,C)
is to
.B ^cmd(F,C)
what
.B ^Space^cmd(D,X)
is to
.BR ^cmd(D,X) .
It copies the selection to the clip buffer, putting it at the beginning or the end in the same way as
.B ^Space^cmd(D,X)
(above).
.TP
.B ^cmd(B,V)
with no selection will paste the current clip buffer's contents.
But in the presence of a selection it performs a more general function:
the contents of the selection and the clip buffer are exchanged.
With a numeric argument,
.B ^cmd(B,V)
pastes or exchanges with a numbered
.IR register ,
which is an alternate clip buffer.
(The main clip buffer is the same as register 0.)
Besides being a means for preserving some text for longer periods
of editing, the registers also serve as a means for extracting
the text that matches a parenthesized subpattern in a regular expression
search.
.SH SEARCHING
.TP
.B ^/
and its synonym
.B ^_
enter search mode.
The variant command
.B ^Space^/
searches for occurrences of POSIX regular expressions.
Each non-command character that is typed thereafter will be appended
to the current search target string and the selection is moved to the
next occurrence thereof.
.P
The case of alphabetic characters is
.I not
significant to the search.
.P
Most command characters will automatically take the editor out of
search mode before executing, and the most recently discovered
occurrence of the search target string will be its selection.
.P
A few commands have different meanings in search mode:
.TP
.B Backspace
will remove the last character from the search target and
move the selection back to its previous position.
.TP
.B ^cmd(V,U)
is typically used to leave search mode with the cursor at the beginning of
the latest hit, with the mark returned to where it was before the search
(if anywhere).
.TP
.B ^/
with no characters in the search target string will cause the
last search's target string to be reused.
.TP
.B ^cmd(H,G)
and
.B ^cmd(T,H)
cause motion to the previous and next occurrences of the search
target string, not single-character motion.
.TP
.B Enter
(and
.BR ^/ )
simply leaves search mode with the last hit as the selection.
.SH TEXTS, VIEWS, and WINDOWS
.TP
.B ^cmd(K,W)
saves
.I all
modified texts back to their files.
.TP
.B ^Space^cmd(K,W)
saves just the current text.
.TP
.B ^cmd(X,E)
with no selection inserts, as the new selection, the path name of the
current text.  With a selection containing a path name,
possibly constructed with the assistance of tab completion (above),
.B ^cmd(X,E)
will raise up a window containing a view into the indicated file,
creating a new text to hold it if one does not already exist.
.TP
.B ^Space^cmd(X,E)
with a selection will rename the current text, so that it will be
saved in another file.
.TP
.B ^cmd(W,F)
finds an invisible view and associates it with the current window,
making its current view invisible.  Hitting
.B ^cmd(W,F)
repeatedly will cycle through all of the views.
If there was no invisible view,
.B ^cmd(W,F)
creates a new scratch text, as does
.B ^Space;
below.
.TP
.B ^Space^cmd(W,F)
does the same thing. but will close the window's current view,
and also its text if it was the last view thereof.
.TP
.B ^cmd(Y,D)
splits the current window horizontally, raising up an invisible
or new view in the lower half of the original window.
.TP
.B ^Space^cmd(Y,D)
splits the current window vertically, raising up an invisible or new
view in the right half of the original window.
.TP
.B ^cmd(P,S)
moves to another window.
.TP
.B ^Space^cmd(P,S)
moves to another window, closing the old one.
.TP
.B ^Space;
(note that
.B ;
is not a control character)
creates a new anonymous text.
.SH MACROS
.TP
.B ^Space^cmd(O,B)
commences the recording of your keystrokes as the current view's local
.IR macro ,
which may contain anything but another macro definition or
local macro invocation.
.TP
.B ^SpaceF1-F12
commences the recording of your keystrokes as a new global macro for a
function key.
Note that
.B F1
and
.B F11
are typically hijacked by window managers for their own purposes and
probably will not be usable.
.TP
.B ^cmd(O,B)
ends the recording of a macro, if one is in progress.
Afterwards,
.B ^cmd(O,B)
replays the view's local macro.
.SH FOLDING
.B AOEUI
supports the "folding" of portions of text into what appear to be
single characters, and the reverse "unfolding" operation.
Further, to provide outline views of texts such as source code
that are heavily indented,
.B AOEUI
has an automatic nested folding capability.
.TP
.B ^Space,
with a selection will fold the selection.
Otherwise, it will repeatedly fold
indented regions of the text to provide an outline view.
A numeric value, if any, specifies the number of leading spaces or
equivalent tabs at which code lines will be folded.
The default is 1, causing the folding of any line that isn't left-justified.
.TP
.B ^Space.
with a selection will unfold the topmost foldings within it.
Otherwise, if there is no numeric value, it will unfold the
single folded region immediately beneath the cursor.
With a numeric value, which is ignored, it will completely
unfold the entire view.
.SH SHELLS
.TP
.B ^cmd(E,R)
with no selection will launch an interactive shell in a new scratch
text.
With a selection, however,
.B ^cmd(E,R)
will execute the shell command in the selection with the contents
of the clip buffer, if any, as its standard input, and collect its
output asynchronously in the background to replace the selection.
This allows many helpful UNIX text processing commands to be
used directly.
Some handy commands to know:
.TP
.BR cat (1)
to include another entire file, or to receive writes to a named pipe
.TP
.BR mkfifo (1)
to create a named pipe so that commands in other windows may direct
their output into a text running
.B cat
in the background.
.TP
.BI "cd " path
to change the editor's current working directory (a special case command
that is not actually passed to a shell)
.TP
.BR grep (1)
to search for lines containing a pattern
.TP
.BR sort (1)
to rearrange lines alphabetically or numerically, possibly reversed
.TP
.BR uniq (1)
to discard duplicated lines
.TP
.BR sed (1)
as in
.B "sed 's/FROM/TO/g'"
to perform unconditional search-and-replace with regular expressions
.TP
.BR tr (1)
to convert lower to upper case with
.B "a-z A-Z"
and to remove DOS carriage returns with
.BR "-d '[\er]'"
.TP
.BR fmt (1)
to reformat paragraphs of natural language text
.TP
.B "indent -st -kr -i8 -nbbo"
to reformat C language source code sensibly
.TP
.B "column -t"
to realign data nicely into columns
.TP
.B "man | colcrt"
to read a man page
.TP
.BR tailf (1)
to monitor additions to a file such as a log
.TP
.BR make (1)
to compile your code
.TP
.B "aspell -l | sort | uniq | column"
to get a list of words that may be misspelled
.B ^Space^cmd(E,R)
with no selection will terminate the output of any asynchronous
child process that's still running.
.SH TIPS
.TP
.B *
To select the rest of the line after the cursor, use
.B ^cmd(V,U)^cmd(C,Y)
.TP
.B *
It is often faster to retype a bungled word than to fix it, using
.B ^cmd(V,U)^cmd(N,K)
and then retyping.
.TP
.B *
Transposing multiple blocks of text is easy with
.BR ^cmd(B,V) ,
which generalized the usual paste operation into an exchange of the clip buffer
with the selection.
.TP
.B *
Incremental search and replacement can be done with a macro or by
clipping the replacement text, and on search hits that are to be
replaced, using
.B ^cmd(B,V)^cmd(F,C)^/^/
to exchange the hit with the replacement text, copy it back to the
clip buffer, and proceed to the next occurrence of the search pattern.
But when the replacement text is short, it's sometimes easiest to just
overwrite the selection by hitting
.B Enter
to leave search mode and immediately retyping it, since the cursor
will be placed at its beginning.
.TP
.B *
Reconfigure your keyboards so that the key to the left of A, which is
probably labeled
.BR "Caps Lock" ,
is interpreted as a Control modifier instead.
.TP
.B *
The
.BR gnome-terminal (1)
terminal emulator works well with
.B AOEUI
if you configure the terminal's scrollback limit to a relatively
small value.
.TP
.B *
To move backward or forward by half a screenfull, use
.B ^cmd(R,O)
or
.B ^cmd(L,P)
and then
.BR ^cmd(Z,N) .
.SH BUGS
Inevitable; please tell me about any that you find.
.SH ENVIRONMENT
.TP
.B SHELL
is used to name the program run by the
.B ^cmd(E,R)
command.
.SH FILES
.TP
.IB file ~
is overwritten with the original contents of
.IR file .
.TP
.IB file #
contains the temporary image of the edited file
while
.B AOEUI
is running, and may be useful in recovery if the editor
is killed.
.TP
.B TAGS
is read in by the
.B ^Space'
command, if not already present, to supply the tags that are
scanned.  It should contain the output of
.B "ctags -x"
.IR files .
.TP
.B $HOME/.aoeui
holds any new "anonymous" texts created during editing sessions.
.SH "SEE ALSO"
.BR cmd(`asdfg',`aoeui') (1),
.BR ctags (1),
.BR regex (7)
.P
Helpful commands to use with
.BR ^cmd(E,R) :
.BR aspell (1),
.BR cat (1),
.BR colcrt (1),
.BR column (1),
.BR fmt (1),
.BR grep (1),
.BR indent (1),
.BR mkfifo (1),
.BR sed (1),
.BR sort (1),
.BR tailf (1),
.BR tr (1),
.BR uniq (1)
.SH AUTHOR
Peter Klausler <peter@klausler.com> wrote
.BR AOEUI .
