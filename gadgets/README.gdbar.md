gdbar
=====

(c) 2007 by Robert Manea

`gdbar` is an application that generates fully graphical progress meters,
from some values you supply to it.

It has the same input semantics as dbar (see README.dbar). In contrast
to dbar, gdbar draws fully graphical meters which are only useful in
combination with dzen >= 0.7.0.

Options:
--------

     -fg  :  foreground color of the meter (default: white)
     -bg  :  background color of the meter (default: darkgrey)
     -s   :  Style, can be either o(utlined), v(ertical),
             or the default gauge if no parameter is specified
     -sw  :  Width of the segments
     -sh  :  Height of the vertical segments
     -ss  :  Space between the segments
     -max :  Value to be considered 100%   (default: 100)
     -min :  Value to be considered   0%   (default: 0  )
     -w   :  Size in pixels to be 
             considered 100% in the meter  (default: 80 )
     -h   :  bar height
     -l   :  label to be prepended to 
             the bar                       (default: '' )
     -o   :  Draw conky style meters
     -nonl:  no new line, don't put 
             '\n' at the end of the bar    (default: do print '\n')


See `dbar` for further details!

