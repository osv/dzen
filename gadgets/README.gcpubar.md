gcpubar
=======

(c) 20007 by Robert Manea

`gcpubar` is a CPU utilization meter for Linux. It relies on the existence
of the "/proc/stat" file and generateѕ fully graphical meters viewable with
dzen >= 0.7.0.

Command line options:
---------------------


- `-i`  :  Update interval in seconds    (default:   1)
           You can use positive values
           less than 1 for intervals 
           shorter than a second

- `-c`  :  Number of times to display    (default: infinite)
           the meter

- `-w`  :  Width of the meter in pixels  (default: 100)

- `-h`  :  Height of the meter in pixels (default:  10)

- `-fg` :  Meter foreground color        (default: white)

- `-bg` :  Meter background color        (default: darkgrey)

- `-s`  :  Style, can be either o(utlined)
           v(ertical), g(raph) or the default
           gauge if no parameter is specified

- `-gw` :  Width of the graph elements    (default: 1)

- `-gs` :  Space between the graph 
            elemements                     (default: 0)

- `-nonl`: no new line, don't put
           `\n` at the end of the bar    (default: do print `\n`)

