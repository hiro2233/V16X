#include "UR_V16X_DeepService.h"
#include <UR_Crypton/UR_Crypton.h>

#include <sys/stat.h>

#define V16X_DIR_TMP v16xtmp

const UR_V16X_DeepService::cmd_lst_t UR_V16X_DeepService::_cmd_lst[] = {
    {"ls"},
    {"cat"},
    {"echo"},
    {NULL},
};

UR_V16X_DeepService::UR_V16X_DeepService() :
    _maxparams(0),
    _cntparamparsed(0)
{
}

UR_V16X_DeepService::UR_V16X_DeepService(const uint16_t maxparams) :
    _maxparams(maxparams),
    _cntparamparsed(0)
{
    _qparams = new query_param_t[_maxparams];
}

UR_V16X_DeepService::~UR_V16X_DeepService(void)
{
    if ((_qparams != NULL) || (_qparams != nullptr)) {
        destroy_qparams(_qparams, _maxparams);
        delete[] _qparams;
    }
}

void UR_V16X_DeepService::print_query_params(const query_param_t *qparam, uint32_t cnt)
{
    if (cnt == 0) {
        return;
    }
    uint32_t idx_param = 0;
    if (has_key(qparam, 0, cnt, "qstr", idx_param)) {
        SHAL_SYSTEM::printf("%sPRINT SDS QSTR key:%s [%s] val: [%s] idx: [%d]\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, \
                            qparam[idx_param].key, qparam[idx_param].val, idx_param);
    }
    if (has_key(qparam, 0, cnt, "qbin", idx_param)) {
        const uint16_t qblen = strlen(qparam[idx_param].val) + 1;
        if (qblen > 0) {
            char qbtmp[qblen] = {0};
            sprintf(qbtmp, "%s", qparam[idx_param].val);

            SHAL_SYSTEM::printf("%sPRINT SDS QBIN key:%s [%s] val: [%s] idx: [%d] len:%d\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, \
                                qparam[idx_param].key, qbtmp, idx_param, qblen);
        }
    }

    SHAL_SYSTEM::printf("%s----------------------------------%s\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET);
    for (uint8_t i = 0; i < cnt; i++) {
        if (qparam[i].key) {
            SHAL_SYSTEM::printf("%s idx:%s %d key: %s ", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, i, qparam[i].key);
            if (qparam[i].val) {
                SHAL_SYSTEM::printf("val: %s ", qparam[i].val);
            }
            SHAL_SYSTEM::printf("\n");
        }
    }
    SHAL_SYSTEM::printf("%s----------------------------------%s\n\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET);
}

uint32_t UR_V16X_DeepService::parse_query(const char *query, char delimiter, char setter, query_param_t *params, uint32_t max_params)
{
    if (strlen(query) == 0) {
        return 0;
    }

    uint32_t i = 0;
    uint32_t idxcnt = 0;
    char querytmp[strlen(query) + 1] = {0};
    char *pch = NULL;
    char delimset[4] = {0, 0, 0, 0};

    sprintf(delimset, "%c%c", delimiter, setter);

    if (NULL == query || 0 == *query) {
        return -1;
    }

    memmove(&querytmp[0], &query[0], strlen(query));
    if (setter == 0) {
        pch = strtok(querytmp, "\r\n");
    } else {
        pch = strtok(querytmp, delimset);
    }

    while (idxcnt < max_params && pch != NULL) {
        if (setter == 0) {
            params[idxcnt].key = new char[strlen(pch) + 1];
            memset(&params[idxcnt].key[0], 0, strlen(pch) + 1);
            memmove(&params[idxcnt].key[0], pch, strlen(pch));
            params[idxcnt].val = NULL;
            pch = strtok(NULL, "\r\n");
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

void UR_V16X_DeepService::parse_query(const char *query, char delimiter, char setter)
{
    if ((_qparams != NULL) || (_qparams != nullptr)) {
        destroy_qparams(_qparams, _maxparams);
    }

    _cntparamparsed = parse_query(query, delimiter, setter, _qparams, _maxparams);
}

bool UR_V16X_DeepService::has_key(const query_param_t *params, uint32_t offset, uint32_t cnt, const char *strkey, uint32_t &idx)
{
    bool ret = false;
    for (uint32_t i = offset; i < cnt; i++) {
        if ((params[i].key == NULL) || (params[i].key == nullptr)) {
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
    if (cnt == 0) {
        return 0;
    }

    uint32_t destroyedcnt = 0;
    for (uint32_t i = 0; i < cnt; i++) {
        if ((params[i].key != NULL) || (params[i].key != nullptr)) {
            delete[] params[i].key;
            params[i].key = NULL;
            destroyedcnt++;
        }
        if ((params[i].val != NULL) || (params[i].val != nullptr)) {
            delete[] params[i].val;
            params[i].val = NULL;
        }
    }

    return destroyedcnt;
}

void UR_V16X_DeepService::_strhex2byte(char *strhex, uint8_t *dest)
{
    uint32_t len = strlen(strhex) / 2;
    uint8_t restmp = 0;
    for (uint32_t i = 0; i < len; i++) {
        char c1[2] = {0, 0};
        memcpy(&c1[0], strhex, 2);
        sscanf(&c1[0], "%2x", (unsigned int*)&restmp);
        SHAL_SYSTEM::printf("[0x%02x] ", restmp);
        dest[i] = restmp;
        strhex = strhex + 2;
    }
    SHAL_SYSTEM::printf("\n");
}

bool UR_V16X_DeepService::process_qparams(const query_param_t *qparams, uint32_t cnt, char **retmsg)
{
    if (cnt == 0) {
        return false;
    }

    bool ret = false;
    uint32_t idx_param = 0;
    uint16_t idx_offset = 0;

    // Process string queries
    for (uint16_t i = 0; i < cnt; i++) {
        if (has_key(qparams, idx_offset, cnt, "qstr", idx_param)) {
            SHAL_SYSTEM::printf("%s\nprocess SDS QSTR key:%s %s val: %s idx: %d\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, \
                                qparams[idx_param].key, qparams[idx_param].val, idx_param);

            uint64_t vallen = strlen(qparams[idx_param].val);
            char valtmp[vallen + 1] = {0};

            memcpy(valtmp, qparams[idx_param].val, vallen);
            uint8_t *ptr = (uint8_t*)&_testdata;

            memset(ptr, 0, sizeof(test_s));
            _strhex2byte(valtmp, ptr);
            SHAL_SYSTEM::printf("%s\nQSTR data %s - d1: %lu  d2: %lu  d3: %lu\n", COLOR_PRINTF_WHITE(1), COLOR_PRINTF_RESET, \
                                (long unsigned int)_testdata.data1, (long unsigned int)_testdata.data2, (long unsigned int)_testdata.data3);
            ret = _execute_qstr(qparams, cnt, retmsg);
            idx_offset = idx_param + 1;
        }
    }

    idx_param = 0;
    idx_offset = 0;

    // Process binary queries
    for (uint16_t i = 0; i < cnt; i++) {
        if (has_key(qparams, idx_offset, cnt, "qbin", idx_param)) {
            char decout[2048] = {0};
            int cntdecout = 0;
            const uint16_t qblen = strlen(qparams[idx_param].val) + 1;
            char qbtmp[qblen] = {0};
            sprintf(qbtmp, "%s", qparams[idx_param].val);

            UR_Crypton urcrypton;
            urcrypton.init();
            cntdecout = urcrypton.b64_dec(decout, qparams[idx_param].val, strlen(qparams[idx_param].val));

            SHAL_SYSTEM::printf("%s\nPROCESS SDS QBIN key:%s %s val: %s decval: %s idx: %d\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, \
                                qparams[idx_param].key, qbtmp, decout, idx_param);

            memset(&_testdata, 0, sizeof(test_s));
            if (cntdecout > 0) {
                memcpy(&_testdata, &decout, sizeof(test_s));
            } else {
                memcpy(&_testdata, &qparams[idx_param].val[0], sizeof(test_s));
            }

//#if V16X_DEBUG >= 1
            uint16_t vallen = sizeof(test_s);

            SHAL_SYSTEM::printf("\tQBIN Encoded:\n\t");
            SHAL_SYSTEM::printf("%s", COLOR_PRINTF_PURPLE(1));
            for (uint16_t j = 0; j < vallen; j++) {
                SHAL_SYSTEM::printf("[0x%02x] ", (uint8_t)qparams[idx_param].val[j]);
            }
            SHAL_SYSTEM::printf("%s", COLOR_PRINTF_RESET);

            SHAL_SYSTEM::printf("\n\n\tQBIN Decoded:\n\t");
            SHAL_SYSTEM::printf("%s", COLOR_PRINTF_PURPLE(1));
            for (uint16_t j = 0; j < vallen; j++) {
                SHAL_SYSTEM::printf("[0x%02x] ", (uint8_t)decout[j]);
            }
            SHAL_SYSTEM::printf("%s", COLOR_PRINTF_RESET);
            SHAL_SYSTEM::printf("\n");
            SHAL_SYSTEM::printf("%s\nQBIN data %s - d1: %lu  d2: %lu  d3: %lu\n",COLOR_PRINTF_WHITE(1), COLOR_PRINTF_RESET, \
                                (long unsigned int)_testdata.data1, (long unsigned int)_testdata.data2, (long unsigned int)_testdata.data3);
//#endif // V16X_DEBUG

            idx_offset = idx_param + 1;
            ret = true;
        }
    }

    return ret;
}

bool UR_V16X_DeepService::_execute_qstr(const query_param_t *qparams, uint32_t cnt, char **retmsg)
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

        // Check if leveling up path and Prevents absolute path operations
        char *vargs = valargstmp;
        if (strstr(vargs, "..")) {
            *vargs = 0x20;
        }

        while (*vargs != 0) {
            if (*vargs == '/') {
                *vargs = ' ';
            }
            vargs++;
        }

        uint64_t vallen = strlen(qparams[idx_cmd].val);
        char valtmp[vallen + 1] = {0};

        memcpy(valtmp, qparams[idx_cmd].val, vallen);

        if (!_cmd_avail(valtmp)) {
            return false;
        }

        if (strlen(valargstmp) <= 0) {
            sprintf(valargstmp, "%s", ".");
        }
        char cmdtmp[strlen(valtmp) + strlen(valargstmp)];
        sprintf(cmdtmp, "%s %s 2>&1", valtmp, valargstmp);

        ret = _exe_cmd(cmdtmp, retmsg);

#if V16X_DEBUG >= 99
        uint64_t keylen = strlen(qparams[idx_cmd].key);
        char keytmp[keylen + 1] = {0};
        memcpy(keytmp, qparams[idx_cmd].key, keylen);
        SHAL_SYSTEM::printf("%sDATA SDS executed key:%s %s val: %s idx: %d vallen: %lu keylen: %lu\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, \
                            keytmp, valtmp, (int)idx_cmd, (long unsigned int)vallen, (long unsigned int)keylen);
#endif // V16X_DEBUG
    }
    return ret;
}

bool UR_V16X_DeepService::_cmd_avail(char *pcmd)
{
    bool ret = false;
    const cmd_lst_t *map = _cmd_lst;

    while(map->cmd) {
        if (strcmp(map->cmd, pcmd) == 0) {
            ret = true;
            break;
        }
        map++;
    }

    return ret;
}

bool UR_V16X_DeepService::_exe_cmd(const char *cmd, char **retmsg)
{
#ifndef __MINGW32__
    int retmkdir = mkdir(STRINGIZEDEF_VAL(V16X_DIR_TMP), 0775);
#else
    int retmkdir = mkdir(STRINGIZEDEF_VAL(V16X_DIR_TMP));
#endif
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

uint32_t UR_V16X_DeepService::parse_query_bin(const char *query, uint32_t querylen, query_param_t *params)
{
    if (querylen == 0) {
        return 0;
    }

    if (NULL == query || 0 == *query) {
        return 0;
    }

    char tokstr[] = "qbin=";
    const uint16_t toklen =  (uint16_t)strlen(tokstr);
    const uint32_t qlenend = querylen - toklen;
    const char *tokfound = strstr(query, tokstr);

    if (tokfound != NULL) {
        tokfound = tokfound + toklen;
//#if V16X_DEBUG >= 1
        SHAL_SYSTEM::printf("\n%sPARSE QBIN pos:%s %ld - tokstr size: %ld tokstr len: %ld qlen %lu qlenend: %lu\n", COLOR_PRINTF_PURPLE(1), COLOR_PRINTF_RESET, \
                            (tokfound - query), sizeof(tokstr), (long int)toklen, (long unsigned int)querylen, (long unsigned int)qlenend);

        SHAL_SYSTEM::printf("%s----------------------------------\n", COLOR_PRINTF_PURPLE(1));
        for (uint32_t i = 0; i < qlenend; i++) {
            SHAL_SYSTEM::printf("[0x%02x] ", tokfound[i]);
        }
        SHAL_SYSTEM::printf("\n----------------------------------%s", COLOR_PRINTF_RESET);
//#endif // V16X_DEBUG
        params[0].key = new char[toklen + 1];
        memset(&params[0].key[0], 0, toklen + 1);
        memmove(&params[0].key[0], tokstr, toklen - 1);

        params[0].val = new char[qlenend + 1];
        memset(&params[0].val[0], 0, qlenend + 1);
        memmove(&params[0].val[0], tokfound, qlenend);
        return 1;
    }

    return 0;
}
