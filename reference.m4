ifdef(`ASDFG',`define(`AOEUI',`asdfg')define(`cmd',`$2')',`define(`AOEUI',`aoeui')define(`cmd',`$1')')dnl
Welcome to AOEUI 1.6! This is reference card.

        ---- no mark ----               ------ mark ------
        raw     ^Space  arg             raw     ^Space  arg
 Q      suspend quit
 cmd(U,Z)      undo    redo
 cmd(K,W)      save    save 1
 \              abort
 cmd(X,E)      (get path)                      visit   (set path)

 cmd(H,G)      <-ch    up      <-chs
 cmd(T,H)      ch->    down    chs->
 cmd(N,K)      <-wd    <-sent  <-wds
 cmd(S,L)      wd->    sent->  wds->
 cmd(G,T)      <-ln    <-pp    <-lns
 cmd(C,Y)      ln->    pp->    lns->
 cmd(R,O)      <-pg    home
 cmd(L,P)      pg->    end
 ]      []
 cmd(Z,N)      center  reset   ->line#
 _      search  regexp

 J      (auto-align new line)
 Tab    (tab complete)                  Tab
 ^      (literal/ctl)   unicode

 cmd(V,U)      mark    selline unmark          unmark  swap    unmark
 cmd(F,C)                                      copy    append  repl
 cmd(D,X)      cutch   selwhite                cut     append  repl
 cmd(B,V)      paste                           exch            register

 cmd(W,F)      view    close
 cmd(Y,D)      split   vsplit                  narrow
 cmd(P,S)      window  close   ->window#

 cmd(O,B)      macro   macstart macros
 cmd(E,R)      shell   endchildren             pipe
 =              bookmk  bookmk#
 -              gotomk  gotomk#
 ;              (new anon text)
 '              (go to tag)
 ,              (fold view)                     (fold selection)
 .              unfold  unfoldall               (unfold selection)
 #              where
 ?              help


Command characters
*  ESC  function keys and query responses, ALT
*  Fk   global macro execute [start; repeat]
  ^@    (^Space)
  ^^    literal, control [; unicode]
* ^[    (ESC or "smaller font")
  ^]    move to corresponding or nearest bracket [AVAILABLE]
*  TAB  tab / tab completion [align; set tab stop]
  ^cmd(P,S)    select another window [closing current; by index]
  ^cmd(Y,D)    split window [vertically] / narrow to selection
  ^cmd(F,C)    AVAILABLE / copy [pre/append; replicate]
  ^cmd(G,T)    backward to line start [paragraph start; multiple lines]
  ^cmd(C,Y)    forward to line end [paragraph end; multiple lines]
  ^cmd(R,O)    backward screen(s) [beginning of view]
  ^cmd(L,P)    forward screen(s) [end of view]
  ^/    (^_)
* ^?    (BCK) delete character before cursor
* ^+    -- larger font
  ^\    [quit without saving]
* ^A    -- reserved by screen(1), synonym for ^/(^_)
  ^cmd(O,B)    macro end, macro execute [macro start; repeat]
  ^cmd(E,R)    shell [end children] / pipe clipbuffer to command
  ^cmd(U,Z)    undo [redo]
* ^I    (TAB)
  ^cmd(D,X)    cut char [select whitespace] / cut [pre/append; replicate]
  ^cmd(H,G)    backward char(s) [up line; multiple chars]
  ^cmd(T,H)    forward char(s) [down line; multiple chars]
  ^cmd(N,K)    backward word(s) [sentence; multiple words]
  ^cmd(S,L)    forward word(s) [sentence; multiple words]
* ^-    -- smaller font in some WMs, otherwise (^_)
* ^_    (^/) incremental search mode [regexp]
*  ENT  (^M) newline
  ^ENT  (^J)
  ^Q    suspend editor [quit]
* ^J    (^ENT) newline with automatic alignment
  ^cmd(K,W)    save all [single]
  ^cmd(X,E)    get path / visit file [set path]
  ^cmd(B,V)    paste / exchange with clip buffer [; register]
* ^M    (ENT)
  ^cmd(W,F)    select other view [closing current]
  ^cmd(V,U)    set/unset mark [select line / exchange mark with cursor; force unset]
  ^cmd(Z,N)    recenter view [single window, reset display; go to line]
  ^SP   (^@) variant, beginning of value

Non-control characters -- commands must be [variants]
  =     [; set bookmark]
  -     [; go to bookmark]
  ;     [new anonymous text]
  '     [go to tag]
  ,     [fold view on indentation] / [fold selection]
  .     [unfold; unfold entire view] / [unfold selection once]
  #     [get current position]
  ?     [help]
  0-9   decimal argument
  x 0-9 a-f A-F         hexadecimal argument

Legend
*   means "must be this character".
()  indicate synonyms
[;] indicate variants activated by ^Space, possibly with value
/   indicates behavior with mark unset / set
