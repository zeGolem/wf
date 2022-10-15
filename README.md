# wf

A UNIX-style utility for running commands on file change.

## Build

### Dependencies

- A C compiler (I use clang, but GCC works too) with c11 support

### Build instructions

1. Download the code

```console
 $ git clone https://github.com/zeGolem/wf.git 
 $ cd wf
```

(or use curl & unzip)

```console
 $ curl https://github.com/zeGolem/wf/archive/refs/heads/master.zip
 $ unzip wf-master.zip
 $ cd wf-master
```

2. Build it!

```console
 $ ./build.sh
```

(if you don't have clang/LLVM on your system, you can use GCC & 
the standard strip)

```console
CC=gcc STRIP=strip ./build.sh
```

### Systemwide install

Just copy `wf` to `/usr/local/bin/`

```console
 $ sudo cp wf /usr/local/bin/
```


## Usage

```
wf [FILE1] <FILE2> <FILE3> <...> -- [COMMAND TO RUN]
```

For example:

```
wf test -- echo test was updated!
```

will print "test was updated" whenever the file "test" gets
written to.

You can also use %F in your command to specify the file name:

```
wf test1 test2 -- echo The file %F was updated.
```

This will print "The file test1 was updated." when `test1` is
written to, and "The file test2 was updated." when `test2` is.
