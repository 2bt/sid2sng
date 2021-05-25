# sid2sng

Converts sid files back to GoatTracker2 sng files.
Note: sid files need to have been generated via GoatTracker2.


## Usage

    usage: ./sid2sng [options...] sid-file [sng-file]
     -nopulse
     -nofilter
     -noinstrvib
     -fixedparams
     -nowavedelay


## FAQ

**I get an error!** Try supplying some of the options. Good luck!

**Where is the pulse wave?** Try it with `-nowavedelay`.


## TODO

+ auto-detect detect missing tables
+ add more options to disable
+ fix bugs
