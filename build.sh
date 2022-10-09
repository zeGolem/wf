#!/bin/sh

set -ex;
clang -glldb -fsanitize=address,undefined -std=c2x -Wall -Wextra wf.c -o wf;

