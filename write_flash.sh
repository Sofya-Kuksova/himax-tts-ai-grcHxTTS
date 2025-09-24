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

# output the command that will be run
echo "Running swdflash.py with binary file: $BINARY_FILE and address: $ADDRESS"

# Run the swdflash.py script with the provided parameters
python3 swd_debugging/swdflash/swdflash.py --bin="$BINARY_FILE" --addr="$ADDRESS"
