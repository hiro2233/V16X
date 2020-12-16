#include "UR_V16X_DeepService.h"

#include <sys/stat.h>

#define V16X_DIR_TMP v16xtmp

const UR_V16X_DeepService::cmd_lst_t UR_V16X_DeepService::cmd_lst[] = {
    {"ls"},
    {"cat"},
    {"echo"},
    {NULL},
};

UR_V16X_DeepService::UR_V16X_DeepService()
{
}

void UR_V16X_DeepService::print_query_params(const query_param_t *qparam, uint32_t cnt)
{
    uint32_t idx_param = 0;
    if (has_key(qparam, 0, cnt, "qstr", idx_param)) {
        SHAL_SYSTEM::printf("%sSDS QSTR key:%s %s val: %s idx: %d\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, qparam[idx_param].key, qparam[idx_param].val, idx_param);
    }
    if (has_key(qparam, 0, cnt, "qbin", idx_param)) {
        SHAL_SYSTEM::printf("%sSDS QBIN key:%s %s val: %s idx: %d\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, qparam[idx_param].key, qparam[idx_param].val, idx_param);
    }

    SHAL_SYSTEM::printf("%s----------------------------------%s\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET);
    for (uint8_t i = 0; i < cnt; i++) {
        if (qparam[i].key) {
            SHAL_SYSTEM::printf("%sidx:%s %d key: %s ", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, i, qparam[i].key);
            if (qparam[i].val) {
                SHAL_SYSTEM::printf("val: %s", qparam[i].val);
            }
            SHAL_SYSTEM::printf("\n");
        }
    }
    SHAL_SYSTEM::printf("%s----------------------------------%s\n\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET);
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
        pch = strtok(querytmp, &delimiter);
    } else {
        pch = strtok(querytmp, delimset);
    }

    while (idxcnt < max_params && pch != NULL) {
        if (setter == 0) {
            params[idxcnt].key = new char[strlen(pch) + 1];
            memset(&params[idxcnt].key[0], 0, strlen(pch) + 1);
            memmove(&params[idxcnt].key[0], pch, strlen(pch));
            params[idxcnt].val = NULL;
            pch = strtok(NULL, &delimiter);
            idxcnt++;
            continue;
        }
        if (i % 2) {
            params[idxcnt].val = new char[strlen(pch) + 1];
            memset(&params[idxcnt].val[0], 0, strlen(pch) + 1);
            memmove(&params[idxcnt].val[0], pch, strlen(pch));
            if (setter != 0) {
                SHAL_SYSTEM::printf ("%sPVal:%s %s\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, pch);
            }
            idxcnt++;
        } else {
            params[idxcnt].key = new char[strlen(pch) + 1];
            memset(&params[idxcnt].key[0], 0, strlen(pch) + 1);
            memmove(&params[idxcnt].key[0], pch, strlen(pch));
            if (setter != 0) {
                SHAL_SYSTEM::printf ("%sPKey:%s %s ", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, pch);
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
#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("%sprocess SDS QSTR key:%s %s val: %s idx: %d\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, qparams[idx_param].key, qparams[idx_param].val, idx_param);
#endif // V16X_DEBUG
        ret = execute_qstr(qparams, cnt, retmsg);
    }
    if (has_key(qparams, 0, cnt, "qbin", idx_param)) {
#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("%sprocess SDS QBIN key:%s %s val: %s idx: %d\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, qparams[idx_param].key, qparams[idx_param].val, idx_param);
#endif
    }

    return ret;
}

bool UR_V16X_DeepService::execute_qstr(const query_param_t *qparams, uint32_t cnt, char **retmsg)
{
    bool ret = false;
    uint32_t idx_cmd = 0;

    if (has_key(qparams, 1, cnt, "cmd", idx_cmd)) {
        uint32_t idx_args = 0;
        uint64_t valargslen = 0;

        if (has_key(qparams, 2, cnt, "args", idx_args)) {
            valargslen = strlen(qparams[idx_args].val);
        }

        char valargstmp[valargslen + 1] = {0};
        if (valargslen > 0) {
            memcpy(valargstmp, qparams[idx_args].val, valargslen);
        }

        // Prevents absolute path operations.
        char *vargs = valargstmp;
        while (*vargs != 0) {
            if (*vargs == '/') {
                *vargs = ' ';
            }
            vargs++;
        }

        uint64_t vallen = strlen(qparams[idx_cmd].val);
        char valtmp[vallen + 1] = {0};

        memcpy(valtmp, qparams[idx_cmd].val, vallen);

        if (!cmd_avail(valtmp)) {
            return false;
        }

        char cmdtmp[strlen(valtmp) + strlen(valargstmp)];
        sprintf(cmdtmp, "%s %s 2>&1", valtmp, valargstmp);

        ret = exe_cmd(cmdtmp, retmsg);

#if V16X_DEBUG >= 99
        uint64_t keylen = strlen(qparams[idx_cmd].key);
        char keytmp[keylen + 1] = {0};
        memcpy(keytmp, qparams[idx_cmd].key, keylen);
        SHAL_SYSTEM::printf("%sDATA SDS executed key:%s %s val: %s idx: %d vallen: %lu keylen: %lu\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, keytmp, valtmp, (int)idx_cmd, vallen, keylen);
#endif // V16X_DEBUG
    }
    return ret;
}

bool UR_V16X_DeepService::cmd_avail(char *pcmd)
{
    bool ret = false;
    const cmd_lst_t *map = cmd_lst;

    while(map->cmd) {
        if (strcmp(map->cmd, pcmd) == 0) {
            ret = true;
            break;
        }
        map++;
    }

    return ret;
}

bool UR_V16X_DeepService::exe_cmd(const char *cmd, char **retmsg)
{
    int retmkdir = mkdir(STRINGIZEDEF_VAL(V16X_DIR_TMP), 0775);
    int retcd = chdir(STRINGIZEDEF_VAL(V16X_DIR_TMP));
    (void)retmkdir;

    FILE *fd;
    fd = popen(cmd, "r");

    retcd = chdir("..");
    (void)retcd;

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

    return true;
}
