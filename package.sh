#!/bin/sh

set -ex;

# Package the program into a gzip'ed tarball

NAME=$1

# Make a release build of the program
./build.sh;

# Create a directory for us to dump files in
mkdir -p $NAME;

# Copy the binary in the package directory
cp wf $NAME/;

# TODO: Should this be outside this script?
# Write the INSTALL instructions in the package
cat > $NAME/INSTALL << EOF
wf -- A UNIX-style utility for running commands on file change

Install instructions
====================

After unpacking this tarball, run the following:

 # cp $NAME/wf /usr/local/bin/

(assuming /usr/local/bin/ is in your \$PATH)
EOF

# Copy the LICENCE in the package
cp LICENSE $NAME/;

# Package all this in the tarball!
tar czvf $NAME.tar.gz $NAME/;
