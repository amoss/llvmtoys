#!/usr/bin/env python3

import re, sys

lines = list(open("make.trace").readlines())

def findArchiveDeps(archive):
    buildStep = f"; ar [^ ]* {archive}"
    found_archives, found_objs = [], []
    for l in lines:
        if re.search(buildStep,l):
            cmds = l.split(';')
            for arg in cmds[3].split()[2:]:
                if arg[-2:]=='.a':
                    found_archives.append(arg)
                elif arg[-2:]=='.o':
                    found_objs.append(arg)
                else:
                    print(f"Problem decoding ar command -> {arg}")
            return found_archives, found_objs
    print(f"ERROR: could not find build step for {archive}")
    sys.exit(-1)

# Extract top-level archives and object files used to build vmlinux
for l in lines:
  if 'link-vmlinux.sh' in l and 'FORCE' in l:
    archives = [ w for w in l.split() if w[-2:]=='.a' ]
    objs    = [ w for w in l.split() if w[-2:]=='.o' ]

# Recursively find the archives and objects used to build archives
objs = set(objs)
archivesDone = set()
while len(archives)>0:
    for a in archives[:]:
        #print(f"Processing {a}")
        archivesDone.add(a)
        archives.remove(a)
        new_a, new_o = findArchiveDeps(a)
        for aa in new_a:
            if not a in archivesDone and not a in archives:
                archives.append(a)
        objs = objs | set(new_o)

for o in objs:
    print(f"units/{o[:-2]}.ll")
