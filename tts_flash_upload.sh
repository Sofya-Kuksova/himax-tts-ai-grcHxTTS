#!/bin/bash

TINY_STORIES=model_zoo/grovety_tts/fully_connected_weights_u55.bin
TINY_STORIES_BASE_ADDR=0x00200000

MODEL_ENCODER_TINY=model_zoo/grovety_tts/encoder_tiny_int8_vela.tflite
MODEL_ENCODER_TINY_BASE_ADDR=0x008F2000

MODEL_DECODER_I16_32=model_zoo/grovety_tts/decoder_32_int16_vela.tflite
MODEL_DECODER_I16_32_BASE_ADDR=0x00973000

PHONEMES_TABLE=model_zoo/grovety_tts/phonemes_table.bin
PHONEMES_TABLE_BASE_ADDR=0x00AD0000

EMBEDDINGS=model_zoo/grovety_tts/embeddings_tiny_qnn.bin
EMBEDDINGS_BASE_ADDR=0x00F09000

source flasher/.flasher_venv/bin/activate

python3 flasher/xmodem/xmodem_send.py --port=$HIMAX_PORT --baudrate=921600 --protocol=xmodem \
    --model="$TINY_STORIES $TINY_STORIES_BASE_ADDR 0x00000" \
    --model="$MODEL_ENCODER_TINY $MODEL_ENCODER_TINY_BASE_ADDR 0x00000" \
    --model="$MODEL_DECODER_I16_32 $MODEL_DECODER_I16_32_BASE_ADDR 0x00000" \
    --model="$PHONEMES_TABLE $PHONEMES_TABLE_BASE_ADDR 0x00000" \
    --model="$EMBEDDINGS $EMBEDDINGS_BASE_ADDR 0x00000"
