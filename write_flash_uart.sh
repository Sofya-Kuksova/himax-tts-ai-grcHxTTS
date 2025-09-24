#!/bin/bash

# chech we have 2 parameters
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <binary_file> <address>"
    exit 1
fi
BINARY_FILE=$1
ADDRESS=$2
# Check if the binary file exists
if [ ! -f "$BINARY_FILE" ]; then
    echo "Binary file not found: $BINARY_FILE"
    exit 1
fi
# Check if the address is in the correct format
if ! [[ $ADDRESS =~ ^0x[0-9a-fA-F]+$ ]]; then
    echo "Address must be in hexadecimal format (e.g., 0x200000)"
    exit 1
fi

source flasher/.flasher_venv/bin/activate
python3 flasher/xmodem/xmodem_send.py --port=$HIMAX_PORT --baudrate=921600 --protocol=xmodem --model="$BINARY_FILE $ADDRESS 0x00000"
