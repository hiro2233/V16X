#include "UR_V16X_DeepService.h"

UR_V16X_DeepService::UR_V16X_DeepService()
{
}

void UR_V16X_DeepService::print_query_params(const query_param_t *qparam, uint32_t cnt)
{
    uint32_t idx_param = 0;
    if (has_key(qparam, 0, cnt, "qstr", idx_param)) {
        SHAL_SYSTEM::printf("SDS QSTR key: %s val: %s idx: %d\n", qparam[idx_param].key, qparam[idx_param].val, idx_param);
    }
    if (has_key(qparam, 0, cnt, "qbin", idx_param)) {
        SHAL_SYSTEM::printf("SDS QBIN key: %s val: %s idx: %d\n", qparam[idx_param].key, qparam[idx_param].val, idx_param);
    }

    SHAL_SYSTEM::printf("----------------------------------\n");
    for (uint8_t i = 0; i < cnt; i++) {
        if (qparam[i].key && qparam[i].val) {
            SHAL_SYSTEM::printf("idx: %d key: %s val: %s\n", i, qparam[i].key, qparam[i].val);
        }
    }
    SHAL_SYSTEM::printf("----------------------------------\n");
}

uint32_t UR_V16X_DeepService::parse_query(const char *query, char delimiter, char setter, query_param_t *params, uint32_t max_params)
{
    uint32_t i = 0;
    uint32_t idxcnt = 0;
    char querytmp[strlen(query) + 1] = {0};
    char *pch;
    char delimset[2] = {delimiter, setter};

    if (NULL == query || '\0' == *query) {
        return -1;
    }

    memcpy(querytmp, query, strlen(query) + 1);
    pch = strtok(querytmp, delimset);

    while (idxcnt < max_params && pch != NULL) {
        if (setter == '\0') {
            params[idxcnt].key = new char[strlen(pch) + 1];
            memcpy(params[idxcnt].key, pch, strlen(pch));
            params[idxcnt].val = NULL;
            pch = strtok(NULL, delimset);
            idxcnt++;
            continue;
        }
        if (i % 2) {
            params[idxcnt].val = new char[strlen(pch) + 1];
            memset(params[idxcnt].val, 0, strlen(pch));
            sprintf(params[idxcnt].val, "%s", pch);
            if (setter != 0) {
                printf ("Val: %s\n", pch);
            }
            idxcnt++;
        } else {
            params[idxcnt].key = new char[strlen(pch) + 1];
            memset(params[idxcnt].key, 0, strlen(pch));
            sprintf(params[idxcnt].key, "%s", pch);
            if (setter != 0) {
                printf ("Key: %s ", pch);
            }
        }
        pch = strtok(NULL, delimset);
        i++;
    }

    return idxcnt;
}

bool UR_V16X_DeepService::has_key(const query_param_t *params, uint32_t offset, uint32_t cnt, const char *strkey, uint32_t &idx)
{
    bool ret = false;
    for (uint8_t i = offset; i < cnt; i++) {
        if (!params[i].key) {
            continue;
        }
        if (strlen(params[i].key)  < 2) {
            continue;
        }

        if (strstr(params[i].key, strkey)) {
            ret = true;
            idx = i;
            break;
        }
    }

    return ret;
}

uint32_t UR_V16X_DeepService::destroy_qparams(query_param_t *params, uint32_t cnt)
{
    uint8_t i;
    for (i = 0; i < cnt; i++) {
        if (params[i].key) {
            delete params[i].key;
        }
        if (params[i].val) {
            delete params[i].val;
        }
    }

    return i;
}
