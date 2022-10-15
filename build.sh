#!/bin/sh

set -ex;

CC=${CC:-clang}
CFLAGS="$CFLAGS -std=c2x"
DEBUGGER=${DEBUGGER:-lldb}
STRIP=${STRIP:-llvm-strip}

if [ "$1" = 'dev' ]; then
	$CC -g$DEBUGGER -fsanitize=address,undefined -Wall -Wextra $CFLAGS wf.c -o wf;
else
	$CC -O2 $CFLAGS wf.c -o wf;
	$STRIP wf
fi;
