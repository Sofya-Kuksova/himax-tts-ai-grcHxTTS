#!/bin/bash

source flasher/.flasher_venv/bin/activate
python3 swd_debugging/swdflash/swdflash.py --bin=flasher/bootloader.img --addr=0x00000000
