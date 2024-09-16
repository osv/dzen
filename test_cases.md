## Test: 1 simple

### Args: -fn Terminus:size=22:antialias=true:hinting=false -h 30 -w 300

[reference](./integration-tests/reference_01.gif)

### Pipe data


```
hello
worl^fg(#0ee)d
```

## Test: 2 simple2

### Args: -fn terminus:size=22:antialias=false:hinting=false -h 30 -w 300

[reference](./integration-tests/reference_02.gif)

### Pipe data

```
^r(100x2)hello
```

## Test: 3 Menu horizontal

### Args: -l 4 -m h -fn terminus:size=22:antialias=false:hinting=false -h 30 -w 300

### Mouse: 10,10

[reference](./integration-tests/reference_03-menu-horizontal-open-popup.gif)
### Pipe data

```
header
line 1
line 2
line 3
line 4
```

## Test: 4 Menu vertical - highlight menu and click

### Args: -l 4 -m -fn terminus:size=22:antialias=false:hinting=false -h 30 -w 300

### Mouse: 10,10
Move mouse to the header to uncollapse menu

### Mouse: 10,61
Move mouse below header to highlight second item (30 * 2 = 60)

### Crop: 300x150+0+0

### Pipe data

```
header
printf "line 1"
printf "line 2"
printf "line 3"
printf "line 4"
```

### Click and check output: 1, line 2

Left mouse button click (1) and check executed command, expected "line 2" 

[reference](./integration-tests/reference_04-menu-vertical-open-popup.gif)
