# Pico Print

# Demo

[<img src="https://pbs.twimg.com/ext_tw_video_thumb/1738269215545565184/pu/img/PkhuoRE5ivJ-Zg5g.jpg" width="300">](https://twitter.com/i/status/1738269349029298260 "GameBoy Advance + ESC/POS printer") [<img src="https://pbs.twimg.com/ext_tw_video_thumb/1736400191505268737/pu/img/FthXAz-TmQqJOZZc.jpg" width="300">](https://twitter.com/i/status/1736402230712926590 "Analogue Pocket + USB printer")


# Build

```
cmake -S . -B build -DPICO_SDK_PATH=~/pico-sdk
cmake --build build
```

# Upload using openocd

```
openocd -c "adapter speed 5000" -f interface/cmsis-dap.cfg -f target/rp2040.cfg -s tcl -c "program build/pico-print.elf verify reset exit"
```

# Stuff you can print in Pokemon games
https://bulbapedia.bulbagarden.net/wiki/Game_Boy_Printer
