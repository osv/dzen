## Test: 01-uniform-inherited

### Args: -x 20 -y 20 -w 300 -h 30 -ta l -bg '#203040' -fg white -b 10

### Pipe data

```
uniform inherited
```

### Geometry: 10,10,320,50
### Title geometry: 20,20,300,30

![reference](./expected/01-uniform-inherited.png)

## Test: 02-asymmetric-explicit

### Args: -x 40 -y 30 -w 300 -h 30 -ta l -bg '#101010' -fg white -b '4,8,12,16,#d03030'

### Pipe data

```
asymmetric explicit
```

### Geometry: 24,26,324,46
### Title geometry: 40,30,300,30

![reference](./expected/02-asymmetric-explicit.png)

## Test: 03-implicit-full-width

### Args: -h 30 -ta l -bg '#182818' -fg white -b '5,10,#308050'

### Pipe data

```
implicit width fills the target
```

### Geometry: 0,0,1920,40
### Title geometry: 10,5,1900,30

![reference](./expected/03-implicit-full-width.png)

## Test: 04-vertical-state

### Args: -x 100 -y 50 -l 2 -w 300 -tw 200 -h 30 -ta l -sa l -bg '#181818' -fg white -b '6,#7050b0' -e 'onstart=uncollapse,grabkeys;button1=collapse;button2=uncollapse;key_h=hide;key_u=unhide'

### Pipe data

```
centered title
first slave line
second slave line
```

### Geometry: 44,44,312,102
### Title geometry: 100,50,200,30
### Slave geometry: 50,80,300,60

![reference](./expected/04-vertical-expanded.png)

### Mouse: 110,15
### Click: 1
### Geometry: 94,44,212,42

![reference](./expected/04-vertical-collapsed.png)

### Click: 2
### Press key: h
### Geometry: 44,44,312,102

![reference](./expected/04-vertical-hidden.png)

### Press key: u

## Test: 05-horizontal-and-hide

### Args: -x 30 -y 40 -l 3 -m h -w 300 -h 30 -bg '#151515' -fg white -b '3,7,9,11,#b08020' -e 'onstart=grabkeys;key_h=hide;key_u=unhide'

### Pipe data

```
header
one
two
three
```

### Geometry: 19,37,318,42
### Slave geometry: 30,40,300,30

![reference](./expected/05-horizontal.png)

### Press key: h
### Geometry: 19,37,318,13

![reference](./expected/05-horizontal-hidden.png)

### Press key: u

## Test: 06-inherited-normbg

### Args: -x 20 -y 20 -w 300 -h 30 -ta l -bg '#202040' -fg white -b 8

### Pipe data

```
before inherited update
```

![reference](./expected/06-inherited-before.png)

### Pipe data

```
^normbg(#206030)
```

![reference](./expected/06-inherited-after.png)

## Test: 07-explicit-normbg

### Args: -x 20 -y 20 -w 300 -h 30 -ta l -bg '#202040' -fg white -b '8,#804020'

### Pipe data

```
before explicit update
```

![reference](./expected/07-explicit-before.png)

### Pipe data

```
^normbg(#206030)
```

![reference](./expected/07-explicit-after.png)

## Test: 08-border-has-no-actions

### Args: -x 20 -y 20 -w 300 -h 30 -ta l -bg '#181818' -fg white -b '10,#305080'

### Pipe data

```
^ca(1,printf content-hit)clickable^ca()
```

### Mouse: 5,15
### Click: 1
### Check no output

### Mouse: 15,15
### Click and check output: 1, content-hit

## Test: 09-slave-above-title

### Args: -x 100 -y 1060 -l 2 -w 300 -h 30 -ta l -sa l -bg '#181818' -fg white -b '5,#406080' -e 'onstart=uncollapse'

### Pipe data

```
title below
slave above one
slave above two
```

### Geometry: 95,980,310,100
### Title geometry: 100,1045,300,30
### Slave geometry: 100,985,300,60

![reference](./expected/09-slave-above-title.png)

## Test: 10-dynamic-expand-static-border

### Args: -x 80 -y 40 -expand right -h 30 -ta l -bg '#181818' -fg white -b '7,#904060'

### Pipe data

```
expanded title
```

![reference](./expected/10-dynamic-expand-static-border.png)
