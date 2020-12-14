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
    char delimset[4];

    sprintf(delimset, "%c%c", delimiter, setter);

    if (NULL == query || 0 == *query) {
        return -1;
    }

    memmove(&querytmp[0], &query[0], strlen(query));
    if (setter == 0) {
        pch = strtok(querytmp, &setter);
    } else {
        pch = strtok(querytmp, delimset);
    }

    while (idxcnt < max_params && pch != NULL) {
        if (setter == 0) {
            params[idxcnt].key = new char[strlen(pch) + 1];
            memset(&params[idxcnt].key[0], 0, strlen(pch) + 1);
            memmove(&params[idxcnt].key[0], pch, strlen(pch));
            params[idxcnt].val = NULL;
            pch = strtok(NULL, &setter);
            idxcnt++;
            continue;
        }
        if (i % 2) {
            params[idxcnt].val = new char[strlen(pch) + 1];
            memset(&params[idxcnt].val[0], 0, strlen(pch) + 1);
            memmove(&params[idxcnt].val[0], pch, strlen(pch));
            if (setter != 0) {
                printf ("Val: %s\n", pch);
            }
            idxcnt++;
        } else {
            params[idxcnt].key = new char[strlen(pch) + 1];
            memset(&params[idxcnt].key[0], 0, strlen(pch) + 1);
            memmove(&params[idxcnt].key[0], pch, strlen(pch));
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
    for (uint32_t i = offset; i < cnt; i++) {
        if (params[i].key == NULL) {
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
    uint32_t i;
    for (i = 0; i < cnt; i++) {
        if (params[i].key) {
            free(params[i].key);
        }
        if (params[i].val) {
            free(params[i].val);
        }
    }

    return i;
}

bool UR_V16X_DeepService::process_qparams(const query_param_t *qparams, uint32_t cnt, char **retmsg)
{
    bool ret = false;
    uint32_t idx_param = 0;

    if (has_key(qparams, 0, cnt, "qstr", idx_param)) {
        SHAL_SYSTEM::printf("process SDS QSTR key: %s val: %s idx: %d\n", qparams[idx_param].key, qparams[idx_param].val, idx_param);
        execute_qstr(qparams, cnt, retmsg);
    }
    if (has_key(qparams, 0, cnt, "qbin", idx_param)) {
        SHAL_SYSTEM::printf("process SDS QBIN key: %s val: %s idx: %d\n", qparams[idx_param].key, qparams[idx_param].val, idx_param);
    }

    return ret;
}

bool UR_V16X_DeepService::execute_qstr(const query_param_t *qparams, uint32_t cnt, char **retmsg)
{
    bool ret = false;
    uint32_t idx_param = 0;
    if (has_key(qparams, 1, cnt, "cmd", idx_param)) {
        FILE *fd;
        uint64_t vallen = strlen(qparams[idx_param].val);
        uint64_t keylen = strlen(qparams[idx_param].key);
        char valtmp[vallen + 1] = {0};
        char keytmp[keylen + 1] = {0};

        memcpy(valtmp, qparams[idx_param].val, vallen);
        memcpy(keytmp, qparams[idx_param].key, keylen);

        fd = popen(valtmp, "r");

        if (!fd) {
            return false;
        }

        char buffer[128];
        size_t chread;
        size_t comalloc = 128;
        size_t comlen   = 0;
        char *msgtmp = (char*)calloc(comalloc, 1);

        while ((chread = fread(buffer, 1, sizeof(buffer), fd)) != 0) {
            if (comlen + chread >= comalloc) {
                comalloc *= 2;
                msgtmp = (char*)realloc(msgtmp, comalloc);
            }
            memmove(msgtmp + comlen, buffer, chread);
            comlen += chread;
        }

        *retmsg = (char*)calloc(comlen, 1);
        memmove(*retmsg, msgtmp, comlen);
        free(msgtmp);
        pclose(fd);

        SHAL_SYSTEM::printf("DATA SDS executed, key: %s val: %s idx: %d vallen: %lu keylen: %lu\n", keytmp, valtmp, (int)idx_param, vallen, keylen);
        ret = true;
    }
    return ret;
}
