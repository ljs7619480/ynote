#!/usr/bin/env python3

import sys
import inspect

def main():
    for it, fi in enumerate(inspect.stack()):
        sys.stdout.write('frame #{}:\n  {}\n\n'.format(it, fi))

if __name__ == '__main__':
    main()
# [end example]

# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
