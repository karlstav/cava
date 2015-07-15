[![Build Status](https://travis-ci.org/ndevilla/iniparser.svg?branch=master)](https://travis-ci.org/ndevilla/iniparser)

# Iniparser 4 #


## I - Overview

This modules offers parsing of ini files from the C level.
See a complete documentation in HTML format, from this directory
open the file html/index.html with any HTML-capable browser.

Key features :

 - Small : around 1500 sloc inside 4 files (2 .c and 2 .h)
 - Portable : no dependancies, written in `-ansi -pedantic` C89
 - Fully reintrant : easy to make it thread-safe (just surround
   library calls by mutex)

## II - Building project

A simple `make` at the root of the project should be enough to get the static
(i.e. `libiniparser.a`) and shared (i.e. `libiniparser.so.0`) libraries compiled.

You should consider trying the following rules too :

 - `make check` : run the unitary tests
 - `make example` : compile the example, run it with `./example/iniexample`

## III - License

This software is released under MIT License.
See LICENSE for full informations

## IV - Versions

Current version is 4.0 which introduces breaking changes in the api.
Older versions 3.1 and 3.2 with the legacy api are available as tags.


## V - FAQ

### Is Iniparser thread safe ?

Starting from version 4, iniparser is designed to be thread-safe, provided you surround it with your own mutex logic.
The choice not to add thread safety inside the library has been done to provide more freedom for the developer, especially when dealing with it own custom reading logic (i.g. acquiring the mutex, reading plenty of entries in iniparser, then releasing the mutex).

### Your build system isn't portable, let me help you...

I have received countless contributions from distrib people to modify the Makefile into what they think is the "standard", which I had to reject.
The default, standard Makefile for Debian bears absolutely no relationship with the one from SuSE or RedHat and there is no possible way to merge them all.
A build system is something so specific to each environment that it is completely pointless to try and push anything that claims to be standard. The provided Makefile in this project is purely here to have something to play with quickly.

