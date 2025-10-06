#ifndef MEMORY_MAP_H_
#define MEMORY_MAP_H_

#include <stdint.h>

/*
  Адреса в виде BASE_ADDR_FLASH1_R_ALIAS + OFFSET.
  OFFSETS согласованы с GRC HxTTS flasher-инструкцией (см. flasher/xmodem).
  Эти оффсеты — физические смещения в flash (которые flasher записывает),
  и в рантайме доступны через BASE_ADDR_FLASH1_R_ALIAS.
*/

/* (опционально) tinyStories LLM — если у вас есть отдельный образ LLM, укажите offset.
   Если LLM (tinyStories) встроен в firmware.img, можно оставить этот адрес соответствующим
   месту, куда packer кладёт LLM; для совместимости указываю 0x00850000 */


/* Используем адреса, которые применяет GRC flasher (xmodem_send examples) */
#define MODEL_ENCODER_TINY_BASE_ADDR    ((void*)(BASE_ADDR_FLASH1_R_ALIAS + 0x00200000))
#define MODEL_DECODER_I16_32_BASE_ADDR  ((void*)(BASE_ADDR_FLASH1_R_ALIAS + 0x00281000))
#define PHONEMES_TABLE_BASE_ADDR        ((void*)(BASE_ADDR_FLASH1_R_ALIAS + 0x00400000))
#define EMBEDDINGS_BASE_ADDR            ((void*)(BASE_ADDR_FLASH1_R_ALIAS + 0x00840000))
#define MODEL_TINY_STORIES_BASE_ADDR    ((void*)(BASE_ADDR_FLASH1_R_ALIAS + 0x00850000))

/* размер таблицы фонем в байтах — оставляем как в репозитории (проверено) */
#define PHONEMES_TABLE_SIZE 4425951

/* arena sizes — оставляем прежними, но при сборке/прошивке проверяйте OOM в runtime */
#define TOTAL_ARENA_SIZE   (0x00200000 - 0x1f000 - 0x7DB0)
#define ENCODER_ARENA_SIZE 0x58500
#define DECODER_ARENA_SIZE 0x1801E0

#endif // MEMORY_MAP_H_
