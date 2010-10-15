#!/bin/bash
echo "foo"
for i in 1 2 3 4; do sleep 1; echo "arg1 $1, arg2 $2, arg4 $4"; done
echo "foo" 1>&2
