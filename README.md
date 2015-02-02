# Decrypt9
### Generating Xorpads for encrypted games using browserhax

You can use http://dukesrg.no-ip.org/3ds/go to load the Launcher.dat on your 3DS.

First build by running `make` in the project root.

Then use `ncchinfo_gen` on your encrypted game (dump it with the Gateway launcher).

Place `ncchinfo.bin` on your SD card, and run the decryptor. It should take a while, especially if you're decrypting a larger game.

### Thanks to

Roxas75 for the method of ARM9 code injection

Cha(N), Kane49, and all other FatFS contributors for FatFS

Normmatt for `sdmc.s` as well as project infrastructure (Makefile, linker setup, etc)

Relys, sbJFn5r for the decryptor
