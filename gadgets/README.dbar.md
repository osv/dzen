dbar, (c) 2007 by Robert Manea
==============================

dbar is an application that generates semi graphical progress meters,
from some values you supply to it.

See the usage examples for a description of the expected input format.

Options:
--------

- `-max` :  Value to be considered 100%   (default: 100)
- `-min` :  Value to be considered   0%   (default: 0  )
- `-w`   :  Number of charcaters to be
            considered 100% in the meter  (default: 25 )
- `-s`   :  Symbol represeting the
            percentage value in the meter (default: =  )
- `-l`   :  label to be prepended to
            the bar                       (default: '' )
- `-nonl`:  no new line, don't put
            `\n` at the end of the bar    (default: do print `\n`)

`dbar` lets you define static 0% and 100% marks with the '-min' and '-max'
options or you can provide these marks dynamically at runtime.  Static
and dynamic marks can be mixed, in this case the value specified at
runtime will override the comandline value
.
You can specify ranges of numbers, negative, positive or ranges with a
negative min value and positive max value.

All numbers are treated as double precision floating point, i.e. the
input is NOT limited to integers.


Usage examples:
---------------

 1) Static 0% and 100% mark or single value input:

        echo 25 | dbar -m 100 -l Sometext
        Output: Sometext  25% [======                   ]

 2) If your 100% mark changes dynamically or 2-values input:

        echo "50 150" | dbar
              ^   ^
              |   |__ max. value
              |
              |__ value to display
        Output: 33% [========                 ]

 3) If your value range is not between `[0, maxval]` or 3-values input:

        echo "50 -25 150" | dbar
              ^   ^  ^
              |   |  |__ max. value 100% mark
              |   |
              |   |_____ min. value 0% mark
              |
              |________ value to display
        Output: 43% [===========              ]

 4) Multiple runs:

        for i in 2 20 50 75 80; do echo $i; sleep 1; done | dbar | dzen2
        Output: Find out yourself.
