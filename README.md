Official BitPenny Client Source Code (www.BitPenny.com)
based on Bitcoin 0.6.0 codebase


Purpose
===================

This source code is released with the intention of allowing community members
to inspect and customize the BitPenny Client in order to improve security and
prevent the possibility of >50% mining power attacks by the server.  
Developers are free to make changes to the code, submit pull requests, and 
develop alternate clients.  The BitPenny Client is meant to be used together 
with the BitPenny mining pool server, which is not open source.  By using this
software, you agree to do so in good faith and to not deliberately harm the
BitPenny server. 


How To Build
===================
	git clone git://github.com/ForceMajeure/BitPenny-Client.git
	cd BitPenny-Client/src
	make -f makefile.bitpenny.unix bitpennyd
 
edit and place bitpenny.conf into the .bitpenny/ directory

For dependencies, see doc/build-unix.txt
