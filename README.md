# kdbtool
A command line tool for merging KeePassX 1.X databases

# USAGE:
./kdbtool merge FILE1.kdb FILE2.kdb OUTFILE.kdb

Will prompt for password (assumes password is same for both files)

# Bugs/missing:
- No support for keyfile (just master password)
- Assumes password is same on both files
- Ordering of groups and entries changes when merging
  - (need to take idx into acct)
- Probably still leftovers from the GUI version that can be removed
  - Excessive #includes
  - Consider De-QTifying the whole thing
- Not sure about multi-platform support
