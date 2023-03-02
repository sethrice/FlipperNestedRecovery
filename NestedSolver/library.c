#include "library.h"
#include "crapto1.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "parity.h"
#include "Python.h"

uint32_t
nonce2key(uint32_t uid, uint32_t nt, uint32_t nr, uint32_t ar, uint64_t par_info, uint64_t ks_info, uint64_t **keys) {
    union {
        struct Crypto1State *states;
        uint64_t *keylist;
    } unionstate;

    uint32_t i, pos;
    uint8_t ks3x[8], par[8][8];
    uint64_t key_recovered;

    // Reset the last three significant bits of the reader nonce
    nr &= 0xFFFFFF1F;

    for (pos = 0; pos < 8; pos++) {
        ks3x[7 - pos] = (ks_info >> (pos * 8)) & 0x0F;
        uint8_t bt = (par_info >> (pos * 8)) & 0xFF;

        par[7 - pos][0] = (bt >> 0) & 1;
        par[7 - pos][1] = (bt >> 1) & 1;
        par[7 - pos][2] = (bt >> 2) & 1;
        par[7 - pos][3] = (bt >> 3) & 1;
        par[7 - pos][4] = (bt >> 4) & 1;
        par[7 - pos][5] = (bt >> 5) & 1;
        par[7 - pos][6] = (bt >> 6) & 1;
        par[7 - pos][7] = (bt >> 7) & 1;
    }

    unionstate.states = lfsr_common_prefix(nr, ar, ks3x, par, (par_info == 0));

    if (!unionstate.states) {
        *keys = NULL;
        return 0;
    }

    for (i = 0; unionstate.keylist[i]; i++) {
        lfsr_rollback_word(unionstate.states + i, uid ^ nt, 0);
        crypto1_get_lfsr(unionstate.states + i, &key_recovered);
        unionstate.keylist[i] = key_recovered;
    }
    unionstate.keylist[i] = -1;

    *keys = unionstate.keylist;
    return i;
}

int compare_uint64(const void *a, const void *b) {
    if (*(uint64_t *) b == *(uint64_t *) a) return 0;
    if (*(uint64_t *) b < *(uint64_t *) a) return 1;
    return -1;
}

// create the intersection (common members) of two sorted lists. Lists are terminated by -1. Result will be in list1. Number of elements is returned.
uint32_t intersection(uint64_t *listA, uint64_t *listB) {
    if (listA == NULL || listB == NULL)
        return 0;

    uint64_t *p1, *p2, *p3;
    p1 = p3 = listA;
    p2 = listB;

    while (*p1 != UINT64_C(-1) && *p2 != UINT64_C(-1)) {
        if (compare_uint64(p1, p2) == 0) {
            *p3++ = *p1++;
            p2++;
        } else {
            while (compare_uint64(p1, p2) < 0) ++p1;
            while (compare_uint64(p1, p2) > 0) ++p2;
        }
    }
    *p3 = UINT64_C(-1);
    return p3 - listA;
}

void num_to_bytes(uint64_t n, size_t len, uint8_t *dest) {
    while (len--) {
        dest[len] = (uint8_t) n;
        n >>= 8;
    }
}

int Compare16Bits(const void *a, const void *b) {
    if ((*(uint64_t *) b & 0x00ff000000ff0000) == (*(uint64_t *) a & 0x00ff000000ff0000)) return 0;
    if ((*(uint64_t *) b & 0x00ff000000ff0000) > (*(uint64_t *) a & 0x00ff000000ff0000)) return 1;
    return -1;
}

typedef struct {
    union {
        struct Crypto1State *slhead;
        uint64_t *keyhead;
    } head;
    union {
        struct Crypto1State *sltail;
        uint64_t *keytail;
    } tail;
    uint32_t len;
    uint32_t uid;
    uint32_t nt_enc;
    uint32_t ks1;
} StateList_t;

char *run_nested(uint32_t uid, uint32_t nt0, uint32_t ks0, uint32_t nt1, uint32_t ks1) {
    char *keys = malloc(sizeof(char *) << 20);
    struct Crypto1State *p1, *p2, *p3, *p4;
    StateList_t statelists[2];

    for (uint8_t i = 0; i < 2; i++) {
        statelists[i].uid = uid;
    }

    statelists[0].nt_enc = nt0;
    statelists[0].ks1 = ks0;

    statelists[1].nt_enc = nt1;
    statelists[1].ks1 = ks1;

    // create and run worker threads
    statelists[0].head.slhead = lfsr_recovery32(statelists[0].ks1, statelists[0].nt_enc ^ statelists[0].uid);
    statelists[1].head.slhead = lfsr_recovery32(statelists[1].ks1, statelists[1].nt_enc ^ statelists[1].uid);

    for (p1 = statelists[0].head.slhead; p1->odd | p1->even; p1++) {

    };

    for (p2 = statelists[1].head.slhead; p2->odd | p2->even; p2++) {

    };
    statelists[0].len = p1 - statelists[0].head.slhead;
    statelists[0].tail.sltail = --p1;

    statelists[1].len = p2 - statelists[1].head.slhead;
    statelists[1].tail.sltail = --p2;

    qsort(statelists[0].head.slhead, statelists[0].len, sizeof(uint64_t), Compare16Bits);
    qsort(statelists[1].head.slhead, statelists[1].len, sizeof(uint64_t), Compare16Bits);

    p1 = p3 = statelists[0].head.slhead;
    p2 = p4 = statelists[1].head.slhead;

    while (p1 <= statelists[0].tail.sltail && p2 <= statelists[1].tail.sltail) {
        if (Compare16Bits(p1, p2) == 0) {

            struct Crypto1State savestate;
            savestate = *p1;
            while (Compare16Bits(p1, &savestate) == 0 && p1 <= statelists[0].tail.sltail) {
                *p3 = *p1;
                lfsr_rollback_word(p3, statelists[0].nt_enc ^ statelists[0].uid, 0);
                p3++;
                p1++;
            }
            savestate = *p2;
            while (Compare16Bits(p2, &savestate) == 0 && p2 <= statelists[1].tail.sltail) {
                *p4 = *p2;
                lfsr_rollback_word(p4, statelists[1].nt_enc ^ statelists[1].uid, 0);
                p4++;
                p2++;
            }
        } else {
            while (Compare16Bits(p1, p2) == -1) p1++;
            while (Compare16Bits(p1, p2) == 1) p2++;
        }
    }

    p3->odd = -1;
    p3->even = -1;
    p4->odd = -1;
    p4->even = -1;
    statelists[0].len = p3 - statelists[0].head.slhead;
    statelists[1].len = p4 - statelists[1].head.slhead;
    statelists[0].tail.sltail = --p3;
    statelists[1].tail.sltail = --p4;

    qsort(statelists[0].head.keyhead, statelists[0].len, sizeof(uint64_t), compare_uint64);
    qsort(statelists[1].head.keyhead, statelists[1].len, sizeof(uint64_t), compare_uint64);
    // Create the intersection
    statelists[0].len = intersection(statelists[0].head.keyhead, statelists[1].head.keyhead);

    uint32_t keycount = statelists[0].len;

    for (uint32_t i = 0; i < keycount; i++) {
        char *ch = malloc(14);
        uint64_t key64 = 0;

        crypto1_get_lfsr(statelists[0].head.slhead + i, &key64);
        snprintf(ch, 14, "%012lx;", key64);
        for (uint32_t j = 0; j < 14; j++) {
            strncat(keys, &ch[j], 1);
        }
    }

    return keys;
}

typedef struct {
    uint32_t first;
    uint32_t second;
    uint32_t uid;
    uint32_t nt0;
    uint32_t ks0;
    uint32_t nt1;
    uint32_t ks1;
    uint8_t *par_array_first;
    uint8_t *par_array_second;
    char *keys;
    bool free;
} InfoList_t;

static int valid_nonce(uint32_t Nt, uint32_t NtEnc, uint32_t Ks1, const uint8_t *parity) {
    return (
                   (oddparity8((Nt >> 24) & 0xFF) == ((parity[0]) ^ oddparity8((NtEnc >> 24) & 0xFF) ^ BIT(Ks1, 16))) && \
               (oddparity8((Nt >> 16) & 0xFF) == ((parity[1]) ^ oddparity8((NtEnc >> 16) & 0xFF) ^ BIT(Ks1, 8))) && \
               (oddparity8((Nt >> 8) & 0xFF) == ((parity[2]) ^ oddparity8((NtEnc >> 8) & 0xFF) ^ BIT(Ks1, 0)))
           ) ? 1 : 0;
}

uint32_t pow_calc(uint32_t num, uint32_t deg) {
    uint32_t result = 1;

    for (long i = 0; i < deg; i++) {
        result *= num;
    }

    return result;
}

uint8_t *decode_parity(uint32_t parity) {
    uint8_t *par_array = malloc(sizeof(uint8_t) * 4);

    for (int j = 3; j >= 0; j--) {
        if (j) {
            par_array[3 - j] = (parity / pow_calc(10, j)) - 1;
        } else {
            par_array[3 - j] = parity - 1;
        }

        parity -= (parity / pow_calc(10, j)) * pow_calc(10, j);
    }

    return par_array;
}

bool nested_calculate(InfoList_t *arg) {
    InfoList_t *info = arg;
    uint32_t first = info->first;
    uint32_t second = info->second;

    struct Crypto1State *p1, *p2, *p3, *p4;
    StateList_t statelists[2];

    for (uint8_t i = 0; i < 2; i++) {
        statelists[i].uid = info->uid;
    }

    statelists[0].nt_enc = prng_successor(info->nt0, first);
    statelists[0].ks1 = info->ks0 ^ statelists[0].nt_enc;

    statelists[1].nt_enc = prng_successor(info->nt1, second);
    statelists[1].ks1 = info->ks1 ^ statelists[1].nt_enc;

    if (!valid_nonce(statelists[0].nt_enc, info->ks0, statelists[0].ks1, info->par_array_first) ||
        !valid_nonce(statelists[1].nt_enc, info->ks1, statelists[1].ks1, info->par_array_second)) {

        return false;
    }

    // create and run worker threads
    statelists[0].head.slhead = lfsr_recovery32(statelists[0].ks1, statelists[0].nt_enc ^ statelists[0].uid);
    statelists[1].head.slhead = lfsr_recovery32(statelists[1].ks1, statelists[1].nt_enc ^ statelists[1].uid);

    for (p1 = statelists[0].head.slhead; p1->odd | p1->even; p1++) {

    };

    for (p2 = statelists[1].head.slhead; p2->odd | p2->even; p2++) {

    };
    statelists[0].len = p1 - statelists[0].head.slhead;
    statelists[0].tail.sltail = --p1;

    statelists[1].len = p2 - statelists[1].head.slhead;
    statelists[1].tail.sltail = --p2;
    qsort(statelists[0].head.slhead, statelists[0].len, sizeof(uint64_t), Compare16Bits);
    qsort(statelists[1].head.slhead, statelists[1].len, sizeof(uint64_t), Compare16Bits);

    p1 = p3 = statelists[0].head.slhead;
    p2 = p4 = statelists[1].head.slhead;

    while (p1 <= statelists[0].tail.sltail && p2 <= statelists[1].tail.sltail) {
        if (Compare16Bits(p1, p2) == 0) {

            struct Crypto1State savestate;
            savestate = *p1;
            while (Compare16Bits(p1, &savestate) == 0 && p1 <= statelists[0].tail.sltail) {
                *p3 = *p1;
                lfsr_rollback_word(p3, statelists[0].nt_enc ^ statelists[0].uid, 0);
                p3++;
                p1++;
            }
            savestate = *p2;
            while (Compare16Bits(p2, &savestate) == 0 && p2 <= statelists[1].tail.sltail) {
                *p4 = *p2;
                lfsr_rollback_word(p4, statelists[1].nt_enc ^ statelists[1].uid, 0);
                p4++;
                p2++;
            }
        } else {
            while (Compare16Bits(p1, p2) == -1) p1++;
            while (Compare16Bits(p1, p2) == 1) p2++;
        }
    }

    p3->odd = -1;
    p3->even = -1;
    p4->odd = -1;
    p4->even = -1;
    statelists[0].len = p3 - statelists[0].head.slhead;
    statelists[1].len = p4 - statelists[1].head.slhead;
    statelists[0].tail.sltail = --p3;
    statelists[1].tail.sltail = --p4;

    qsort(statelists[0].head.keyhead, statelists[0].len, sizeof(uint64_t), compare_uint64);
    qsort(statelists[1].head.keyhead, statelists[1].len, sizeof(uint64_t), compare_uint64);
    // Create the intersection
    statelists[0].len = intersection(statelists[0].head.keyhead, statelists[1].head.keyhead);

    if (!info->free) {
        for (uint32_t i = 0; i < statelists[0].len; i++) {
            char *ch = malloc(14);
            uint64_t key64 = 0;

            crypto1_get_lfsr(statelists[0].head.slhead + i, &key64);
            snprintf(ch, 14, "%012lx;", key64);
            for (uint32_t j = 0; j < 14; j++) {
                strncat(info->keys, &ch[j], 1);
            }
        }
    }

    if (statelists[0].len) {
        return true;
    } else {
        return false;
    }
}

void *nested_wrapper(void *arg) {
    if (nested_calculate(arg)) {
        return (void *) 1;
    } else {
        return 0;
    }
}

char *run_full_nested(uint32_t uid, uint32_t nt0, uint32_t ks0, uint32_t par0, uint32_t nt1, uint32_t ks1,
                          uint32_t par1, int from, int to) {
    if (ks0 == ks1) {
        return calloc(1, 1);
    }
    pthread_t thread_id[to];
    uint32_t found_second[to];
    InfoList_t *found_info = malloc(sizeof(InfoList_t) * 2);
    bool found = false;
    uint32_t i;
    for (int first = from; first < to; first++) {
        i = 0;
        void *status = NULL;
//        printf("Trying %u\n", first);
        for (int second = from; second < to; second++) {
            InfoList_t *info = malloc(sizeof(InfoList_t));
            info->first = first;
            info->second = second;
            info->uid = uid;
            info->nt0 = nt0;
            info->ks0 = ks0;
            info->nt1 = nt1;
            info->ks1 = ks1;
            info->free = true;
            uint8_t *par_array_first = decode_parity(par0);
            uint8_t *par_array_second = decode_parity(par1);
            info->par_array_first = par_array_first;
            info->par_array_second = par_array_second;
            found_second[i] = second;
            pthread_create(&thread_id[i], NULL, nested_wrapper, info);
            i++;
        }


        for (uint32_t j = 0; j < i; j++) {
            pthread_join(thread_id[j], &status);
            if (status != 0) {
                found_info->first = first;
                found_info->second = found_second[j];
                found = true;
            }
        }

        if (found) {
            found_info->uid = uid;
            found_info->nt0 = nt0;
            found_info->ks0 = ks0;
            found_info->nt1 = nt1;
            found_info->ks1 = ks1;
            found_info->keys = calloc(256, sizeof(char) * 14);
            uint8_t *par_array_first = decode_parity(par0);
            uint8_t *par_array_second = decode_parity(par1);
            found_info->par_array_first = par_array_first;
            found_info->par_array_second = par_array_second;
            found_info->free = false;
            nested_calculate(found_info);
            return found_info->keys;
        }
    }

    return calloc(1, 1);
}

static PyObject* run_nested_python(PyObject* self, PyObject* args){
    uint64_t uid, nt0, ks0, nt1, ks1;
    if (!PyArg_ParseTuple(args, "lllll", &uid, &nt0, &ks0, &nt1, &ks1)) {
        return NULL;
    }

    char* output = run_nested((uint32_t)uid, (uint32_t)nt0, (uint32_t)ks0, (uint32_t)nt1, (uint32_t)ks1);

    return Py_BuildValue("s", output);
}

static PyObject* run_full_nested_python(PyObject* self, PyObject* args){
    uint64_t uid, nt0, ks0, par0, nt1, ks1, par1;
    int from, to;
    if (!PyArg_ParseTuple(args, "lllllllii", &uid, &nt0, &ks0, &par0, &nt1, &ks1, &par1, &from, &to)) {
        return NULL;
    }

    char* output = run_full_nested((uint32_t)uid, (uint32_t)nt0, (uint32_t)ks0, (uint32_t)par0, (uint32_t)nt1, (uint32_t)ks1, (uint32_t)par1, from, to);

    return Py_BuildValue("s", output);
}

static PyMethodDef nested_methods[] = {
    {"run_nested",  run_nested_python, METH_VARARGS,
     "Run nested"},
    {"run_full_nested",  run_full_nested_python, METH_VARARGS,
     "Run full nested"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef nested_module = {
    PyModuleDef_HEAD_INIT,
    "nested",
    NULL,
    -1,
    nested_methods
};

PyMODINIT_FUNC PyInit_nested(void){
    return PyModule_Create(&nested_module);
}