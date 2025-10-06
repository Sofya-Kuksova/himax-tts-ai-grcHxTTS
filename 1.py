#!/usr/bin/env python3
import os, hashlib

# пути — измените при необходимости
IMG = "we2_image_gen_local/output_case1_sec_wlcsp/output.img"
files = [
    ("encoder", "model_zoo/grovety_tts/encoder_tiny_int8_vela.tflite", 0x00200000),
    ("decoder", "model_zoo/grovety_tts/decoder_32_int16_vela.tflite", 0x00281000),
    ("phonemes", "model_zoo/grovety_tts/phonemes_table.bin", 0x00400000),
    ("embeddings", "model_zoo/grovety_tts/embeddings_tiny_qnn.bin", 0x00840000),
    ("fcw", "EPII_CM55M_APP_S/app/scenario_app/grovety_tts/stories/tinystories.cpp/stories_7m_fc_ethos-u55-64/fully_connected_weights.bin", 0x00850000)
]

def md5_of_file(path):
    h=hashlib.md5()
    with open(path,"rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()

if not os.path.exists(IMG):
    print("ERROR: output image not found:", IMG)
    exit(1)

# compute sizes and ends
info=[]
for name,path,start in files:
    if not os.path.exists(path):
        print("WARNING: model missing:", path)
        size=0
    else:
        size=os.path.getsize(path)
    end = start + size
    info.append((name,path,start,size,end))

# print and check overlaps
print("Layout:")
for name,path,start,size,end in info:
    print(f"{name:10} start={hex(start)} size={size} end={hex(end)}")


# Better printing properly:
for name,path,start,size,end in info:
    print(f"{name:10} start={hex(start)} size={size} end={hex(end)}")

print("\nChecking overlaps:")
for i in range(len(info)-1):
    name_i,_,start_i,size_i,end_i = info[i]
    name_j,_,start_j,_,_ = info[i+1]
    if end_i > start_j:
        print(f"  OVERLAP: {name_i} (end {hex(end_i)}) > {name_j} (start {hex(start_j)})")

# Extract segments from output.img and compare md5 if files exist
with open(IMG,"rb") as imgh:
    for name,path,start,size,end in info:
        if size==0 or not os.path.exists(path):
            continue
        imgh.seek(start)
        seg = imgh.read(size)
        # save temp
        tmp=f"dump_{name}.bin"
        with open(tmp,"wb") as out:
            out.write(seg)
        md_img=hashlib.md5(seg).hexdigest()
        md_local=md5_of_file(path)
        print(f"{name:10} md_img={md_img} md_local={md_local} match={'YES' if md_img==md_local else 'NO'}")
