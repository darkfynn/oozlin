#!/bin/sh

# Create shared library's object file, libooz.o.

gcc -fPIC -Wall -g -c libooz.c

# Create shared library.
# Use -lc to link it against C library, since libooz
# depends on the C library.

gcc -g -shared -Wl,-soname,libooz.so.0 -o libooz.so.0.0 libooz.o -lc 

# At this point we could just copy libooz.so.0.0 into
# some directory, say /usr/local/lib.

# Now we need to call ldconfig to fix up the symbolic links.
 
# Set up the soname.  We could just execute:
# ln -sf libooz.so.0.0 libhooz.so.0
# but let's let ldconfig figure it out.

/sbin/ldconfig -n .

# Set up the linker name.
# In a more sophisticated setting, we'd need to make
# sure that if there was an existing linker name,
# and if so, check if it should stay or not.

ln -sf libooz.so.0 libooz.so

