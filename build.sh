#!/bin/sh

cc -o main main.c -g $(pkg-config --cflags --libs cairo) -lm
