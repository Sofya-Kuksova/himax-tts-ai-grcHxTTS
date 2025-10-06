# Flasher instructions

## Flash map

|  address   | model type            | model                                        | size      | size hex |
| ---------- | --------------------- | -------------------------------------------- | --------- | -------- |
| 0x00200000 | tinyStories LLM       | fully_connected_weights_u55.bin              | 7.4 MB    | 714380   |
| 0x00915000 | yolo8n                | yolov8n_full_integer_quant_vela.tflite       | 2.2 MB    | 21BFD0   |
| 0x00B31000 | detection             | lpd_ssdlite_ocr_no_postprocess_2_vela.tflite | 2.66 MB   | 2A7C20   |
| 0x00DD9000 | detection postprocess | TFLite_Detection_PostProcess_float.tflite    | 25.58 KB  | 6650     |
| 0x00DE0000 | recognition           | lpr_256_no_ctc_10k_vela.tflite               | 845.73 KB | d36f0    |
| 0x00DE0000 | recognition_cn        | lpr_ch_vela.tflite                           | 1.3 MB    | 13F5A0   |

## Setup

```bash
python3 -m venv .flasher_venv
source .flasher_venv/bin/activate
pip install -r requirements.txt
```

## Model upload

```bash
source .flasher_venv/bin/activate
export HIMAX_PORT=/dev/ttyACM0

python3 xmodem/xmodem_send.py --port=$HIMAX_PORT --baudrate=921600 --protocol=xmodem --model="../../himax/EPII_CM55M_APP_S/app/scenario_app/llama2/stories/model/fully_connected_weights_u55.bin 0x00200000 0x00000"

python3 xmodem/xmodem_send.py --port=$HIMAX_PORT --baudrate=921600 --protocol=xmodem --model="../../models/yolo/yolov8n_full_integer_quant_vela.tflite 0x00915000 0x00000"

python3 xmodem/xmodem_send.py --port=$HIMAX_PORT --baudrate=921600 --protocol=xmodem --model="../../models/lpd/lpd_ssdlite_ocr_no_postprocess_2_vela.tflite 0x00B31000 0x00000"
python3 xmodem/xmodem_send.py --port=$HIMAX_PORT --baudrate=921600 --protocol=xmodem --model="../../models/lpd/TFLite_Detection_PostProcess_float.tflite 0x00DD9000 0x00000"
python3 xmodem/xmodem_send.py --port=$HIMAX_PORT --baudrate=921600 --protocol=xmodem --model="../../models/lpr/chinese/lpr_ch_vela.tflite 0x00DE0000 0x00000"
# python3 xmodem/xmodem_send.py --port=$HIMAX_PORT --baudrate=921600 --protocol=xmodem --model="../../models/lpr/lpr_256_no_ctc_10k_vela.tflite 0x200000 0x00000"

```

## Firmware upload

```bash
source .flasher_venv/bin/activate
export HIMAX_PORT=/dev/ttyACM0

python3 flasher.py $HIMAX_PORT --file_path=output.img
```
