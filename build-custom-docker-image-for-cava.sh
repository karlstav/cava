#!/bin/bash

: ${RPXC_IMAGE:=rpxc-with-extras}

docker build -t $RPXC_IMAGE .
