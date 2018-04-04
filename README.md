README
======

This is a new version of Superfastmatch written in C++ to improve matching performance and with an index running totally in memory to improve response times. This is a fork from the original (https://github.com/mediastandardstrust/superfastmatch) with updates for Ubuntu LTS in 2018.

The point of the software is to index large amounts of text in memory. Therefore there isn't much reason to run it on a 32-bit OS with a 4GB cap on memory and a 64-bit OS is assumed

The process for installation on Ubuntu is as follows:


Prerequisites
-------------

First:

    sudo apt install libunwind8-dev mercurial curl build-essential zlib1g-dev libsparsehash-dev liblzo2-dev libre2-dev libkyototycoon-dev cmake-curses-gui

Finally, run:

    sudo ldconfig
    

Setup 
-----

Run:

    ./scripts/bootstrap.sh

and wait for everything to build. The script will ask you for your sudo password, which is required to install the libraries.

Test
----

After the libraries are installed, you can run:

    make check

to run the unit tests for the code.

Build
-----

After that you can run:

    make run

to get a superfastmatch instance running. Nothing is currently configurable from the command line yet. Coming soon...

Visit http://127.0.0.1:8080 to test the interface.

Data
----

For a quick introduction to what can be found with superfastmatch try this:

If you have a machine with less than 8GB of memory and less than 4 cores run:

    ./superfastmatch -debug -hash_width 24 -reset -slot_count 2 -thread_count 2 -window_size 30

otherwise this will be much faster:

    ./superfastmatch -debug -reset -window_size 30

And then finally, in another terminal window, run:

    ./scripts/gutenberg.sh
    
to load some example documents and associate them with each other. You can view the results in the browser at:

    http://127.0.0.1:8080/document/

Daemonizing
-----------

See contrib/init.d for an example init.d script. Makes use of fuser which may require:

    sudo apt-get install psmisc

Feedback
--------

All feedback welcome. Either create an issue [here](https://github.com/mediastandardstrust/superfastmatch/issues) or ask a question on the [mailing list](http://groups.google.com/group/superfastmatch).

Known Issues
------------

This is still an early release halfway between Alpha and Beta! There are known issues with large documents affecting the document list and detail pages and the full REST specification is not yet implemented. Lots of fixes, new features and performance improvements are currently in development so keep checking the commit log!

Acknowledgements
----------------

This is a fork from the original (https://github.com/mediastandardstrust/superfastmatch)

Thanks to [Martin Moore](http://martinjemoore.com/) and [Ben Campbell](http://scumways.com) at [Media Standards Trust](http://mediastandardstrust.org) for ongoing support for the project and to [Tom Lee](http://sunlightfoundation.com/people/tlee/), [Drew Vogel](http://sunlightfoundation.com/people/dvogel/), [Kaitlin Lee](http://sunlightfoundation.com/people/klee/) and [James Turk](http://sunlightfoundation.com/people/jturk/) at [Sunlight Labs](http://sunlightlabs.com) for being willing testers, early adopters and proponents of open source!

Thanks also to [Mikio Hirabayashi](http://fallabs.com) for assistance and the excellent open source [Kyoto Cabinet](http://fallabs.com/kyotocabinet/) and [Kyoto Tycoon](http://fallabs.com/kyototycoon/), to [Craig Silverstein](http://code.google.com/u/@VxVXRFZYBRdBWwU%3D/) for accepting and improving this [patch](http://code.google.com/p/google-sparsehash/source/detail?r=76), to [Neil Fraser](http://neil.fraser.name/) for useful hints and inspiration from [Diff-Match-Patch](http://code.google.com/p/google-diff-match-patch/) and to [Austin Appleby](http://code.google.com/p/smhasher/) for hashing advice.



