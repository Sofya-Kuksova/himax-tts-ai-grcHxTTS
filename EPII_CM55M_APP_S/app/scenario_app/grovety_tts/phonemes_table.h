#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef PHONEMESTABLE_H_
#define PHONEMESTABLE_H_

class PhonemesTable
{
public:
    struct values_view_t {
        const uint8_t* data;
        int32_t len;
    };

    PhonemesTable(void* table_address, size_t table_size) noexcept
    {
        bin_table_data_ = reinterpret_cast<const uint8_t*>(table_address);
        const uint8_t* data_ptr = bin_table_data_;
        entries_num_ = *reinterpret_cast<const int32_t*>(data_ptr);
        data_ptr += sizeof(int32_t);

        entries_ = reinterpret_cast<const entry_t*>(data_ptr);

        int32_t keys_pool_length = 0;
        int32_t values_pool_length = 0;
        if (entries_num_ > 0) {
            keys_pool_length = entries_[entries_num_ - 1].key_off;
            values_pool_length = entries_[entries_num_ - 1].val_off;
        }

        printf("entries_num=%ld\n", entries_num_);
        printf("keys_pool_length=%ld\n", keys_pool_length);
        printf("values_pool_length=%ld\n", values_pool_length);

        data_ptr += entries_num_ * sizeof(entry_t);
        keys_pool_ = data_ptr;
        data_ptr += keys_pool_length;
        values_pool_ = data_ptr;
        data_ptr += values_pool_length;

        assert(data_ptr == bin_table_data_ + table_size);
    }

    values_view_t LookupByKey(const char* key)
    {
        if (! key || entries_num_ <= 1) {
            return {nullptr, 0};
        }

        // Number of actual key-value entries (last entry is for pool sizes)
        const int32_t n = entries_num_ - 1;
        int32_t lo = 0;
        int32_t hi = n - 1; // Last valid key index

        while (lo <= hi) {
            int32_t mid = (lo + hi) / 2;

            const entry_t& entry = entries_[mid];
            // Calculate key length using next entry (mid+1 exists because mid <= n-2 when hi=n-1)
            const int32_t key_len = entries_[mid + 1].key_off - entry.key_off;

            int cmp = compare_key(key, keys_pool_, entry.key_off, key_len);
            if (cmp == 0) {
                // Found match: calculate value length using next entry
                const int32_t val_len = entries_[mid + 1].val_off - entry.val_off;
                return {.data = &values_pool_[entry.val_off], .len = val_len};
            } else if (cmp < 0) {
                hi = mid - 1;
            } else {
                lo = mid + 1;
            }
        }

        return {nullptr, 0};
    }

private:
    struct entry_t {
        int32_t key_off;
        int32_t val_off;
    };

    // Helper function to compare a null-terminated key with a table key (non-null-terminated)
    static int compare_key(const char* key, const uint8_t* keys_pool, int32_t key_offset, int32_t key_len)
    {
        for (int i = 0;; ++i) {
            char c1 = key[i];
            char c2 = (i < key_len) ? keys_pool[key_offset + i] : '\0';
            if (c1 == '\0') {
                return (i < key_len) ? -1 : 0;
            }
            if (i >= key_len) {
                return 1;
            }
            if (c1 != c2) {
                return (c1 < c2) ? -1 : 1;
            }
        }
    }

    const entry_t* entries_;
    int32_t entries_num_;
    const uint8_t* keys_pool_;
    const uint8_t* values_pool_;
    const uint8_t* bin_table_data_;
};

#endif // PHONEMESTABLE_H_
