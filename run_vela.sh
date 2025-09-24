#!/bin/bash

#chech we have 1 parameter
if [ "$#" -ne 1 ]; then
    echo "Usage: ./run_vela.sh <model_file>"
    exit 1
fi

vela --config=vela_config.ini --accelerator-config=ethos-u55-64 --system-config=HiMax --memory-mode=sram_1_5mb $1