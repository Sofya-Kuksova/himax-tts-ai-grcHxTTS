#pragma once


#define USE_SHARED_CLASSIFIER   1
#define MODEL_DIM               288
#define MODEL_HIDDEN_DIM        768
#define MODEL_NUM_LAYERS        6
#define MODEL_NUM_HEADS         6
#define MODEL_NUM_KV_HEADS      6
#define MODEL_VOCAB_SIZE        4096
#define MODEL_VOCAB_MAX_LEN     14
#define MODEL_SEQ_LENGTH        128

#define MODEL_HEAD_SIZE         (MODEL_DIM / MODEL_NUM_HEADS)
#define MODEL_KV_DIM            ((MODEL_DIM * MODEL_NUM_KV_HEADS) / MODEL_NUM_HEADS)
