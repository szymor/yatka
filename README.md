## YATKA
yet another tetris klone in action

### about
- gfx and code (mostly) by me
- background image taken from Berserk manga
- frame limiting, counting and upscaling code by Artur 'zear' Rojek
- arcade font by Yuji Adachi
- in-game music by Vomitron

### how to build (Ubuntu)
    apt update
    apt install libsdl-image1.2-dev libsdl-mixer1.2-dev libsdl-ttf2.0-dev libsdl1.2-dev
    git clone https://github.com/szymor/yatka.git
    cd yatka
    make

### command line parameters
- --nosound - disable music and sound effects
- --scale2x - 640x480 mode
- --scale3x - 960x720 mode
- --scale4x - 1280x960 mode
- --holdoff - disable HOLD function
- --startlevel <num> - the higher level, the higher speed

For more, please refer to the source code.

### plans
- better scoring (T-spins, combos, etc.)
- better wall kick
- better gfx (tetrominoes, animations, etc.)
