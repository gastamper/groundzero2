# groundzeromud

Ground Zero was a MUD (multi-user dungeon) which was popular in the late 90s.
The goals of this project are as follows:
1. Fix obvious bugs.
2. Enforce some standardization in the databases:
    * Keywords on objects should be obvious.
    * Some sanity brought to coloring on objects and mobiles.
    * Clean up in-game text where it is a bit immature.
3. Add in-game documentation where it doesn't exist.
4. Correct in-game documentation where it is erroneous or incomprehensible.

The end goal is to leave the GZ codebase in a better place than I found it so
if anyone else with a nostalgia kick comes along it should be relatively simple
to pick it up and make it work.

#### Things you need to make this work:
1. A *NIX machine to run it all (MacOS apparently works according to master owner)
2. gcc, make, and a semi-standard set of libraries (base CentOS install works)
3. [libcares](https://c-ares.haxx.se/)

#### How to make it work
1. Clone/download this repository
2. Build and install libcares
3. Make the necessary source changes according to the instructions in groundzeromud/src/README
4. Build the server by going into groundzeromud/src and hitting __make__
5. Hit __./ground0__ and pray
