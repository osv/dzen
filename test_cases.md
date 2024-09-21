## Test: 1 Color

### Args: -fn "DejaVu Sans Mono:size=16:dpi=96:spacing=100:style=Book:antialias=true:hinting=false:rgba=none" -h 30 -w 300 -bg '#000' -fg '#fff'

### Pipe data

```
hello
wor^fg(#f00)l^fg(#0f0)d
```

![reference](./integration-tests/reference_01-color.png)


## Test: 2 Rects

### Args: -fn  "DejaVu Sans Mono:size=16:dpi=96:spacing=100:style=Book:antialias=true:hinting=false:rgba=none" -h 30 -w 300 -bg '#000' -fg '#fff'

### Pipe data

```
^ib(1)^fg(red)^ro(100x15)^p(-98)^fg(blue)^r(20x10)^fg(orange)^p(3)^r(40x10)^p(4)^fg(darkgreen)^co(12)^p(2)^c(10)
```

![reference](./integration-tests/reference_02-rects.png)

## Test: 3 Menu horizontal

### Args: -l 4 -m h -fn  "DejaVu Sans Mono:size=16:dpi=96:spacing=100:style=Book:antialias=true:hinting=false:rgba=none" -h 30 -w 300 -bg '#000' -fg '#fff'

### Mouse: 70,10

Mouse over second the item

### Pipe data

Items for menu, first line is invisible header

```
header
line 1
line 2
line 3
line 4
```

![reference](./integration-tests/reference_03-menu-horizontal-highlight.png)


## Test: 4 Menu vertical

### Args: -l 4 -m -fn  "DejaVu Sans Mono:size=16:dpi=96:spacing=100:style=Book:antialias=true:hinting=false:rgba=none" -h 30 -w 300 -bg '#000' -fg '#fff'

### Pipe data

Add some items to menu

```
header
printf "line 1"
printf "line 2"
printf "line 3"
printf "line 4"
```

Should be visible only header

![reference](./integration-tests/reference_04-menu-0-header.png)

### Crop: 300x150+0+0

### Mouse: 10,10

Mouse over the header to uncollapse menu

![reference](./integration-tests/reference_04-menu-1-unclollapse.png)

### Mouse: 10,61

Mouse over the second item (30 * 2 = 60)

![reference](./integration-tests/reference_04-menu-3-second-item.png)

### Click and check output: 1, line 2

Left mouse button click (1) and check executed command, expected "line 2" 

### Mouse: 10,91

Move over the 3rd item

![reference](./integration-tests/reference_04-menu-4-3rd-item.png)

### Pipe data

Add more line, expected scrolled

```
^tw()Changed Header
printf "line 5"
printf "line 6"
```

![reference](./integration-tests/reference_04-menu-5-changed-header.png)

### Pipe data

Clear slave

```
^cs()
printf "line 7"
```

![reference](./integration-tests/reference_04-menu-6-clear-slave.png)


## Test: 5 Position

### Args: -l 5 -e onstart=uncollapse -fn "DejaVu Sans Mono:size=10:dpi=96:spacing=100:style=Book:antialias=true:hinting=false:rgba=none" -h 30 -w 300 -bg '#000' -fg '#fff'

### Pipe data

Not bug but feature:
- Padding after `_TOP` 2px
- `_BOTTOM`: 3px

```
DEF^r(1x16)^r(10x1)^r(1x16)^fg(red)^p(_TOP)^r(1x16)^r(10x1)^r(1x16)Top^r(1x16)^fg(green)^p(_BOTTOM)^p(0;-16)^r(1x16)^r(10x1)^r(1x16)_BOTTOM -16^r(1x16)^fg(#ff0)^p()^r(1x16)^r(10x1)^r(1x16)Reset
TOP: 2px ^r(1x16)^p(_TOP)^fg(red)^r(1x16)
Top 4 vlines^fg(red)^p(_TOP)^r(1x16)^p(_TOP)^r(1x16)^p(_TOP)^r(1x16)^p(_TOP)^r(1x16)
Bottom -16px: 3px: ^r(1x16)^p(_BOTTOM)^p(0;-16)^fg(green)^r(1x16)
Bottom 4 vlines ^fg(green)^p(_BOTTOM)^r(1x16+0-16)^p(_BOTTOM)^r(1x16+0-16)^p(_BOTTOM)^r(1x16+0-16)^p(_BOTTOM)^r(1x16+0-16)
_LOCK_X:^ib(1)^p(_LOCK_X)^ro(40x28)^ro(36x24+2)^ro(32x20+2)^p(_UNLOCK_X)
```

### Crop: 300x180+0+0

![reference](./integration-tests/reference_05-position-padding.png)

## Test: 6 Left align

### Args: -ta l -fn "DejaVu Sans Mono:size=16:dpi=96:spacing=100:style=Book:antialias=true:hinting=false:rgba=none" -h 30 -w 300 -bg '#000' -fg '#fff'

Used `-ta l`

### Pipe data

Absolute 150px

```
Text^pa(150)Abs150
```

![reference](./integration-tests/reference_06-1-left-abs150.png)

### Pipe data

With `^p(_LEFT)` 

```
SomeText^p(_LEFT)^fg(green)left
```

![reference](./integration-tests/reference_06-2-left-left.png)

### Pipe data

With `^p(_CENTER)`

```
SomeText^p(_CENTER)^fg(green)CENTER
```

![reference](./integration-tests/reference_06-3-left-center.png)

### Pipe data

With `^p(_RIGHT)`

```
SomeText^p(_RIGHT)^fg(green)RIGHT
```

![reference](./integration-tests/reference_06-4-left-right.png)

## Test: 7 Center align

### Args: -ta c -fn "DejaVu Sans Mono:size=16:dpi=96:spacing=100:style=Book:antialias=true:hinting=false:rgba=none" -h 30 -w 300 -bg '#000' -fg '#fff'

Used `-ta c`

### Pipe data

Absolute 150px

```
Text^pa(150)Abs150
```

![reference](./integration-tests/reference_07-1-center-abs150.png)

### Pipe data

With `^p(_LEFT)` 

```
SomeText^p(_LEFT)^fg(green)left
```

![reference](./integration-tests/reference_07-2-center-left.png)

### Pipe data

With `^p(_CENTER)`

```
SomeText^p(_CENTER)^fg(green)CENTER
```

![reference](./integration-tests/reference_07-3-center-center.png)

### Pipe data

With `^p(_RIGHT)`

```
SomeText^p(_RIGHT)^fg(green)RIGHT
```

![reference](./integration-tests/reference_07-4-center-right.png)

## Test: 8 Right align

### Args: -ta r -fn "DejaVu Sans Mono:size=16:dpi=96:spacing=100:style=Book:antialias=true:hinting=false:rgba=none" -h 30 -w 300 -bg '#000' -fg '#fff'

Used `-ta r`

### Pipe data

Absolute 150px

```
Text^pa(150)Abs150
```

![reference](./integration-tests/reference_08-1-right-abs150.png)

### Pipe data

With `^p(_LEFT)` 

```
SomeText^p(_LEFT)^fg(green)left
```

![reference](./integration-tests/reference_08-2-right-left.png)

### Pipe data

With `^p(_CENTER)`

```
SomeText^p(_CENTER)^fg(green)CENTER
```

![reference](./integration-tests/reference_08-3-right-center.png)

### Pipe data

With `^p(_RIGHT)`

```
SomeText^p(_RIGHT)^fg(green)RIGHT
```

![reference](./integration-tests/reference_08-4-right-right.png)

## Test: 9 Block area

### Args: -l 5 -e onstart=uncollapse -fn "DejaVu Sans Mono:size=16:dpi=96:spacing=100:style=Book:antialias=true:hinting=false:rgba=none" -h 30 -w 300 -bg '#000' -fg '#fff'


### Pipe data

Block area `^ba` support only plain text align, broken ba means there is some other command.

```

Lef      :^ib(1)^ro(50x28)^p(-50)^ba(50,_LEFT)L
Center   :^ib(1)^ro(50x28)^p(-50)^ba(50,_CENTER)C
Right    :^ib(1)^ro(50x28)^p(-50)^ba(50,_RIGHT)R
Broken ba:^ib(1)^ro(50x28)^p(-50)^ba(50,_CENTER)^fg(green)C^ba()
Broken ba:^ib(1)^ro(50x28)^p(-50)^ba(50,_RIGHT)^fg(green)R^ba()
```

### Crop: 300x180+0+0

![reference](./integration-tests/reference_09-block-area.png)

## Test: 10 Click Area

### Args: -ta l -fn "DejaVu Sans Mono:size=16:dpi=96:spacing=100:style=Book:antialias=true:hinting=false:rgba=none" -h 30 -w 300 -bg '#000' -fg '#fff'


### Pipe data

2 Sensetive Areas.
Area1:
      
      x1=0, x2=99
      y1 = 7, y2 = 23

Because font is Y centered, (30 - 16) / 2 = 7px offset from top&bottom.

Area2:
 
      x1=100, x2=199

```
^ca(1, printf "area1")^bg(#050)^ba(100,_CENTER)area1^ca()^ca(1, printf "area2")^bg(#550)^ba(100,_CENTER)area2^ca()
```

### Mouse: 99,6

### Click and check output: 1,

No area

### Mouse: 99,7

### Click and check output: 1, area1 

Area1

### Mouse: 99,23

### Click and check output: 1, area1 

Area1

### Mouse: 99,24

### Click and check output: 1, 

No Area

### Mouse: 100,23

### Click and check output: 1, area2 

Area2

### Mouse: 199,23

### Click and check output: 1, area2 

Area2, last bottom pixel

### Mouse: 200,23

### Click and check output: 1, 

No area

## Test: 11 Icons

### Args: -fn "DejaVu Sans Mono:size=16:dpi=96:spacing=100:style=Book:antialias=true:hinting=false:rgba=none" -h 30 -w 300 -bg '#000' -fg '#fff'

### Pipe data

```
^fg(green)^i(bitmaps/envelope.xbm)^i(bitmaps/battery_on.xpm)
```

![reference](./integration-tests/reference_11-icons.png)
