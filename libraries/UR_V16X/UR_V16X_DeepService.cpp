#include "UR_V16X_DeepService.h"

#include <sys/stat.h>

#define V16X_DIR_TMP v16xtmp

const UR_V16X_DeepService::cmd_lst_t UR_V16X_DeepService::_cmd_lst[] = {
    {"ls"},
    {"cat"},
    {"echo"},
    {NULL},
};

UR_V16X_DeepService::UR_V16X_DeepService() :
    _maxparams(0)
{
}

UR_V16X_DeepService::UR_V16X_DeepService(const uint16_t maxparams) :
    _maxparams(maxparams)
{
    _qparams = new query_param_t[_maxparams];
}

UR_V16X_DeepService::~UR_V16X_DeepService(void)
{
    if ((_qparams != NULL) || (_qparams != nullptr)) {
        destroy_qparams(_qparams, _maxparams);
    }
    delete[] _qparams;
}

void UR_V16X_DeepService::print_query_params(const query_param_t *qparam, uint32_t cnt)
{
    uint32_t idx_param = 0;
    if (has_key(qparam, 0, cnt, "qstr", idx_param)) {
        SHAL_SYSTEM::printf("%sSDS QSTR key:%s [%s] val: [%s] idx: [%d]\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, \
                            qparam[idx_param].key, qparam[idx_param].val, idx_param);
    }
    if (has_key(qparam, 0, cnt, "qbin", idx_param)) {
        SHAL_SYSTEM::printf("%sSDS QBIN key:%s [%s] val: [%s] idx: [%d]\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, \
                            qparam[idx_param].key, qparam[idx_param].val, idx_param);
    }

    SHAL_SYSTEM::printf("%s----------------------------------%s\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET);
    for (uint8_t i = 0; i < cnt; i++) {
        if (qparam[i].key) {
            SHAL_SYSTEM::printf("%s idx:%s %d key: %s ", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, i, qparam[i].key);
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
    uint32_t destroyedcnt;
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
    bool ret = false;
    uint32_t idx_param = 0;

    // Process string queries
    if (has_key(qparams, 0, cnt, "qstr", idx_param)) {
        SHAL_SYSTEM::printf("%sprocess SDS QSTR key:%s %s val: %s idx: %d\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, \
                            qparams[idx_param].key, qparams[idx_param].val, idx_param);
        uint64_t vallen = strlen(qparams[idx_param].val);
        char valtmp[vallen + 1] = {0};

        memcpy(valtmp, qparams[idx_param].val, vallen);

        uint8_t *ptr = (uint8_t*)&_testdata;

        memset(ptr, 0, sizeof(test_s));
        _strhex2byte(valtmp, ptr);
        SHAL_SYSTEM::printf("%s QSTR websocket %s - d1: %lu  d2: %lu  d3: %lu\n",COLOR_PRINTF_WHITE(1), COLOR_PRINTF_RESET, \
                            (long unsigned int)_testdata.data1, (long unsigned int)_testdata.data2, (long unsigned int)_testdata.data3);
        ret = _execute_qstr(qparams, cnt, retmsg);
    }

    // Process binary queries
    if (has_key(qparams, 0, cnt, "qbin", idx_param)) {
        SHAL_SYSTEM::printf("%s process SDS QBIN key:%s %s val: %s idx: %d\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, \
                            qparams[idx_param].key, qparams[idx_param].val, idx_param);

        memset(&_testdata, 0, sizeof(test_s));
        memcpy(&_testdata, &qparams[idx_param].val[0], sizeof(test_s));

        uint16_t vallen = sizeof(test_s);

        for (uint16_t i = 0; i < vallen; i++) {
            SHAL_SYSTEM::printf("[0x%02x] ", (uint8_t)qparams[idx_param].val[i]);
        }
        SHAL_SYSTEM::printf("\n");
        SHAL_SYSTEM::printf("%s QBIN websocket %s - d1: %lu  d2: %lu  d3: %lu\n",COLOR_PRINTF_WHITE(1), COLOR_PRINTF_RESET, \
                            (long unsigned int)_testdata.data1, (long unsigned int)_testdata.data2, (long unsigned int)_testdata.data3);
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

uint32_t UR_V16X_DeepService::parse_query_bin(const char *query, uint32_t querylen, char delimiter, char setter, query_param_t *params, uint32_t max_params)
{
    if (querylen == 0) {
        return 0;
    }

    uint32_t isel = 0;
    uint32_t idxcnt = 0;
    char querytmp[querylen + 2] = {0};
    char *pch = NULL;
    char delimset[4] = {0, 0, 0, 0};
    char *save_ptr = NULL;
    int32_t lentoken = 0;
    char tokenfound = 0;
    char *tokenextrap = NULL;
    int32_t lentokenextra = 0;

    sprintf(delimset, "%c%c", delimiter, setter);

    if (NULL == query || 0 == *query) {
        return -1;
    }

    memset(&querytmp[0], 0, querylen + 2);
    memmove(&querytmp[0], &query[0], querylen);
    querytmp[querylen + 1] = 'X';

    if (setter == 0) {
        pch = strtok_rdata(querytmp, &delimiter, &save_ptr, lentoken, tokenfound, &tokenextrap, lentokenextra);
    } else {
        pch = strtok_rdata(querytmp, delimset, &save_ptr, lentoken, tokenfound, &tokenextrap, lentokenextra);
    }

    if (tokenfound == 0) {
        char *endpstr = strrchr(pch, 'X');
#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("\nendpstr: %p - %p endpstr: %s len: %ld\n", endpstr, pch, endpstr, (long int)(endpstr - pch));
#endif // V16X_DEBUG
        if ((endpstr - pch) > 0) {
            pch[endpstr - pch - 1] = 0;
            lentoken = lentoken - 2;
        }
    }

    if (tokenextrap != NULL) {
        for (int32_t i = 0; i < lentokenextra; i++) {
            tokenextrap[i] = 0;
        }
    }
#if V16X_DEBUG >= 99
    SHAL_SYSTEM::printf("--------[ found: %s lentoken: %d tokenfound: %c tokenextrap: %s ]--------\n", pch, lentoken, tokenfound, tokenextrap);
#endif // V16X_DEBUG
    if ((idxcnt < max_params) && (pch != NULL) && (save_ptr != NULL)) {
        params[idxcnt].key = new char[lentoken + 1];
        memset(&params[idxcnt].key[0], 0, lentoken + 1);
        memmove(&params[idxcnt].key[0], pch, lentoken);
#if V16X_DEBUG >= 99
        if (setter != 0) {
            SHAL_SYSTEM::printf("%sPKey:%s %s \n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, pch);
        }
#endif // V16X_DEBUG
        isel++;
    }

    while ((idxcnt < max_params) && (pch != NULL) && (save_ptr != NULL)) {
        if (setter == 0) {
            lentoken = 0;
            tokenfound = 0;
            tokenextrap = NULL;
            pch = strtok_rdata(NULL, &delimiter, &save_ptr, lentoken, tokenfound, &tokenextrap, lentokenextra);

            if (tokenfound == 0) {
                char *endpstr = strrchr(pch, 'X');
#if V16X_DEBUG >= 99
                SHAL_SYSTEM::printf("\nendpstr: %p - %p endpstr: %s len: %ld\n", endpstr, pch, endpstr, (long int)(endpstr - pch));
#endif // V16X_DEBUG
                if ((endpstr - pch) > 0) {
                    pch[endpstr - pch - 1] = 0;
                    lentoken = lentoken - 2;
                }
            }

            if (tokenextrap != NULL) {
                for (int32_t i = 0; i < lentokenextra; i++) {
                    tokenextrap[i] = 0;
                }
            }
#if V16X_DEBUG >= 99
            SHAL_SYSTEM::printf("--------[ found: %s lentoken: %d tokenfound: %c tokenextrap: %s ]--------\n", pch, lentoken, tokenfound, tokenextrap);
#endif // V16X_DEBUG

            params[idxcnt].key = new char[lentoken + 1];
            memset(&params[idxcnt].key[0], 0, lentoken + 1);
            memmove(&params[idxcnt].key[0], pch, lentoken);
            params[idxcnt].val = NULL;
            idxcnt++;
            continue;
        }

        lentoken = 0;
        tokenfound = 0;
        tokenextrap = NULL;
        pch = strtok_rdata(NULL, delimset, &save_ptr, lentoken, tokenfound, &tokenextrap, lentokenextra);

        if (tokenfound == 0) {
            char *endpstr = strrchr(pch, 'X');
#if V16X_DEBUG >= 99
            SHAL_SYSTEM::printf("\nendpstr: %p - %p endpstr: %s len: %ld\n", endpstr, pch, endpstr, (long int)(endpstr - pch));
#endif // V16X_DEBUG
            if ((endpstr - pch) > 0) {
                pch[endpstr - pch - 1] = 0;
                lentoken = lentoken - 2;
            }
        }

        if (tokenextrap != NULL) {
            for (int32_t i = 0; i < lentokenextra; i++) {
                tokenextrap[i] = 0;
            }
        }
#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("--------[ found: %s lentoken: %d tokenfound: %c tokenextrap: %s ]--------\n", pch, lentoken, tokenfound, tokenextrap);
#endif // V16X_DEBUG
        if (isel % 2) {
            params[idxcnt].val = new char[lentoken + 1];
            memset(&params[idxcnt].val[0], 0, lentoken + 1);
            memmove(&params[idxcnt].val[0], pch, lentoken);
#if V16X_DEBUG >= 99
            if (setter != 0) {
                SHAL_SYSTEM::printf("%sPVal:%s %s\n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, pch);
            }
#endif // V16X_DEBUG
            idxcnt++;
        } else {
            params[idxcnt].key = new char[lentoken + 1];
            memset(&params[idxcnt].key[0], 0, lentoken + 1);
            memmove(&params[idxcnt].key[0], pch, lentoken);
#if V16X_DEBUG >= 99
            if (setter != 0) {
                SHAL_SYSTEM::printf("%sPKey:%s %s \n", COLOR_PRINTF_BLUE(1), COLOR_PRINTF_RESET, pch);
            }
#endif // V16X_DEBUG
        }
        isel++;
    }

    return idxcnt;
}

char *UR_V16X_DeepService::strtok_rdata(char *str, const char *delim, char **save_ptr, int32_t &lentoken, char &tokenfound, char **tokenextrap, int32_t &lentokenextra)
{
    char *token;

    if (str == NULL) {
        str = *save_ptr;
    }

    // Scan delimiters and return the delimiter found position, if none,
    // then find
    long int strpos = (long int)strcspn(str, delim);
#if V16X_DEBUG >= 99
    SHAL_SYSTEM::printf("\nstrA: %s | saveptr: %s strpos: %ld \"%c\"\n", str, *save_ptr, (long int)strpos, str[strpos]);
    fflush(stdout);
#endif // V16X_DEBUG
    uint32_t lendat = strlen(str);
    token = str;

    char *toktmp = NULL;

    if (str[strpos] == 0) {
        toktmp = str;
#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("\n\nstr[strpos]A: [0x%02x] | pos: %ld\n", str[strpos - 1], strpos);
#endif // V16X_DEBUG
        token = &str[strpos];
        *tokenextrap = &str[strpos];

        while (*token == '\0') {
            *token = '*';
            token++;
            lentokenextra++;
#if V16X_DEBUG >= 99
            if (token != NULL) {
                SHAL_SYSTEM::printf("token: [0x%02x]\n", *token);
            } else {
                SHAL_SYSTEM::printf("token: %s\n", "NULL");
            }
#endif // V16X_DEBUG
        }
#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("token dat: [0x%02x]\n", *token);
#endif // V16X_DEBUG
        str = token;
        strpos = (long int)strcspn(str, delim);
#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("str[strpos]B: [0x%02x] *str:[%c] | toktmp: %s | pos: %ld\n", str[strpos], *str, toktmp, strpos);
#endif // V16X_DEBUG
        lendat = strlen(toktmp);

        token = &str[strpos];
#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("TOKEN: %s | STR: %s \n", token, str);
#endif // V16X_DEBUG
    }

    tokenfound = str[strpos];
#if V16X_DEBUG >= 99
    SHAL_SYSTEM::printf("[*** tokenfound: [0x%02x] | lentoken: %d ***]\n", tokenfound, lentoken);
#endif // V16X_DEBUG
    if (*str == '\0') {
#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("strB: %s\n", *save_ptr);
        fflush(stdout);
#endif // V16X_DEBUG
        return NULL;
    }

    // Find the end of the token.
    str = strpbrk(token, delim);
#if V16X_DEBUG >= 99
    SHAL_SYSTEM::printf("strC: %s\n", str);
#endif // V16X_DEBUG

    if (str == NULL) {
        // This token finishes the string.
        token = *save_ptr;
        toktmp = *save_ptr;
        token[lendat] = 0;
        toktmp[lendat] = 0;
#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("strD: %s - len %ld lendat: %ld\n", *save_ptr, (long int)strlen(*save_ptr), (long int)lendat);
#endif // V16X_DEBUG
        *save_ptr = str;
    } else {
        // Terminate the token and make *SAVE_PTR point past it.
        *str = '\0';
        *save_ptr = str + 1;
#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("strE: %s\n", *save_ptr);
#endif // V16X_DEBUG
    }

    if (toktmp != NULL) {
#if V16X_DEBUG >= 99
        SHAL_SYSTEM::printf("toktmp: %s\n", toktmp);
#endif // V16X_DEBUG
        token = toktmp;
    }

    lentoken = (strlen(token) - lentokenextra) + lentokenextra;
#if V16X_DEBUG >= 99
    SHAL_SYSTEM::printf("token: %s strpos: %ld lentoken: %d tokenextrap: %s\n", token, strpos, lentoken, *tokenextrap);
    fflush(stdout);
#endif // V16X_DEBUG
    return token;
}
