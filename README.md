# Pico Print

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
