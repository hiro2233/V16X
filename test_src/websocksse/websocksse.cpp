#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <pthread.h>

#include "external.h"

test_s testdata;
int default_port = 8080;
int connfd = 0;
int retconn = 0;
int counter = 0;
bool sw_internal = true;

const char *datastr[] = {
    "test1_NATIVE_STREAM_resp",
    "test2_NATIVE_STREAM_resp",
};

/**************************************************************************
 *
 *                      Native Section Code
 *
 **************************************************************************/

extern "C" {

    void EXPORTFUNC dataops();
    int EXPORTFUNC connect_to_deepservice(char ip[], int len, int port);
    void EXPORTFUNC send_deepservice_ping();
    void EXPORTFUNC console_printf(char dat[]);
    int EXPORTFUNC get_counter();
    int EXPORTFUNC Sum(int a, int b);
    int EXPORTFUNC Sum_ccall(int a, int b);

    void EXPORTFUNC set_websock_response(char resp[], int len);
    const char** EXPORTFUNC set_stream_response(char *resp, int len, int &ret);

    void EXPORTFUNC set_element_fps_angle(float fps, float angle);
    char* EXPORTFUNC strrchr_js(char *str, int character);
}

int EXPORTFUNC Sum(int a, int b)
{
    int sumab = a + b;
    return sumab;
}

int EXPORTFUNC Sum_ccall(int a, int b)
{
    int sumab = a + b;
    return sumab;
}

void EXPORTFUNC console_printf(char dat[])
{
    printf("%s\n", dat);
}

int EXPORTFUNC get_counter()
{
    return counter;
}

void EXPORTFUNC send_deepservice_ping()
{
    printf("ping: %d\n", counter);
    counter++;
}

int EXPORTFUNC connect_to_deepservice(char ip[], int len, int port)
{
    /*
        struct sockaddr_in serveraddr;
        char *iptmp = new char[len + 1];

        memset(iptmp, 0, len + 1);
        memset(&serveraddr, 0, sizeof(serveraddr));
        memcpy(iptmp, ip, len);

        if ((connfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("connect failed 1\n");
        }
        fcntl(connfd, F_SETFL, O_NONBLOCK);

        memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        //inet_aton("127.0.0.1", &serveraddr.sin_addr.s_addr);
        serveraddr.sin_addr.s_addr = inet_addr(iptmp);
        //serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
        serveraddr.sin_port = htons((unsigned short)port);

        if (connect(connfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0) {
            printf("connect failed 2 fd: %d\n", connfd);
        }

        printf("Connected\n");
        retconn = 1;
    */
    return retconn;
}

void EXPORTFUNC dataops()
{
    /*
        printf("Native: open and read preloaded data.txt chars with fgetc in while\n");
        FILE *file = fopen("data.txt", "r+");
        while (!feof(file)) {
            char c = fgetc(file);
            if (c != EOF) {
                putchar(c);
            }
        }
        fclose(file);

        int fd;
        int result = 0;
        struct stat st;

        printf("\nNative: WRITING /home/web_user/test/data1.txt\n");
        if ((stat("/home/web_user/test/data1.txt", &st) != -1) || (errno != ENOENT)) {
            result = -4000 - errno;
        }

        fd = open("/home/web_user/test/data1.txt", O_RDWR | O_CREAT, 0666);

        if (fd == -1) {
            result = -5000 - errno;
        } else {
            lseek(fd, 0, SEEK_END);
            const char *cdat = "hello from file!\n";
            if (write(fd, cdat, strlen(cdat)) != 2) {
                result = -6000 - errno;
            }
            if (close(fd) != 0) {
                result = -7000 - errno;
            }
        }

        printf("Native: reading /home/web_user/test/data1.txt\n");
        file = fopen("/home/web_user/test/data1.txt", "r+");
        while (!feof(file)) {
            char c = fgetc(file);
            if (c != EOF) {
                putchar(c);
            }
        }
        fclose(file);

        printf("Native: printed /home/web_user/test/data1.txt\n");
        unlink("/home/web_user/test/data1.txt");
        printf("Native: REMOVED /home/web_user/test/data1.txt\n\n");

        EM_ASM(
            FS.syncfs(function (err) {
            });
        );
    */
}

void EXPORTFUNC set_websock_response(char resp[], int len)
{
    memcpy(&testdata, &resp[0], sizeof(test_s));
    printf("[NATIV SOCK RESP] data1: %d data2: %d data3: %d\n", testdata.data1, testdata.data2, testdata.data3);
}

const char** EXPORTFUNC set_stream_response(char *resp, int len, int &ret)
{

    EM_ASM({
        var data = UTF8ToString(HEAP32[($0 >> 2)]);
        console.log("[STREAM EM_ASM RESP: ]", data);
    }, &resp);

    printf("[STREAM RESP]: %s -  len: %lu\n", &resp[0], strlen(resp));
    ret = ARRAY_SIZE_P(datastr);

    return datastr;
}

char* EXPORTFUNC strrchr_js(char *str, int character)
{
    return strrchr(str, character);
}

/**************************************************************************
 *
 *                          EM_JS Section Code
 *
 **************************************************************************/

//EM_JS(void, set_element_fps_angle, (float fps, float angle), {
void EXPORTFUNC set_element_fps_angle(float fps, float angle)
{
    EM_ASM({
        var element_fps = document.getElementById('fps');
        var element_angle = document.getElementById('angle');
        var result_fps = Math.round(($0 * 100) / 100);

        element_fps.innerHTML = ($0 * globals.get_counter()).toFixed(2);
        element_angle.innerHTML = ($1 * globals.get_counter()).toFixed(2);
    }, fps, angle);
};

EM_JS(char*, basename_js, (const char *name), {
    strrchr_js = Module.cwrap('strrchr_js', 'string', ['string', 'number']);
    var datdot = strrchr_js(name, '.'.charCodeAt(0));
    var cutstr = name.slice(0, name.length - datdot.length);

    var datslash = strrchr_js(cutstr, '/'.charCodeAt(0));
    datslash = datslash.slice(1, datslash.length);

    if (datslash.length == 0)
    {
        datslash = cutstr;
    }

    return datslash;
});

EM_JS(void, on_send_sds_failed, (), {
    globals.on_send_sds_failed();
});

EM_JS(void, import_modules, (const char **modules, int modcnt), {
    var res = [];
    res.push(modules);

    if (typeof(modules) == "number")
    {
        var arr = [];
        for (var i = 0; i < modcnt; i++) {
            var dat = UTF8ToString(HEAP32[(modules >> 2) + i]);
            if (dat.length > 0) {
                arr.push(dat);
            }
        }
        res = arr;
    }

    //console.log("import_modules:", res, res.length, basename_js(res[0]), typeof(modules));

    var xmlhttp_system = [];

    function xmlhttp_response()
    {
        if (this.readyState == 4 && this.status == 200) {
            var obj_response = this.responseText;
            var imported = document.createElement('script');
            imported.type = "application/javascript";
            imported.id = this.idtag;
            imported.text = obj_response;

            console.log("IMPORT OK:", this.filename, "idtag:", this.idtag);

            document.getElementsByTagName('body')[0].appendChild(imported);
        } else {
            if (this.readyState == 4 && this.status == 404) {
                console.log("ERROR IMPORT:", this.filename);
            }
        }
    };

    for (var i = 0; i < res.length; i++)
    {
        var filename = res[i];

        xmlhttp_system.push(new XMLHttpRequest());

        xmlhttp_system[i].filename = filename;
        xmlhttp_system[i].idtag = basename_js(filename);
        xmlhttp_system[i].onreadystatechange = xmlhttp_response;
        xmlhttp_system[i].overrideMimeType("application/javascript");
        xmlhttp_system[i].open("GET", filename, true);
        xmlhttp_system[i].send();
    }

});

/*
EM_JS(void, make_dirsops, (), {
    try
    {
        FS.mkdir('/home/web_user/test');
        FS.mount(IDBFS, {}, '/home/web_user/test');
    } catch (error)
    {
    }

    FS.syncfs(true, function (err)
    {
        var data = new Uint8Array([48, 48, 51, 13]);
        var stream = FS.open('/home/web_user/test/data1.txt', 'a+');
        //FS.llseek(stream, 0, 2);
        globals.console_printf("FS: Writing Uint8Array data.");
        FS.write(stream, data, 0, data.length);
        FS.close(stream);
        globals.console_printf("FS: Uint8Array data written.");

        globals.console_printf("FS: Writing String data.");
        FS.writeFile("/home/web_user/test/data1.txt", "1234\n", { flags: "a+" });

        var contents = FS.readFile('/home/web_user/test/data1.txt', { encoding: 'utf8' });
        globals.console_printf("FS: data readed:\n" + contents.slice());
        globals.console_printf("FS: String data written.");
        FS.syncfs(function (err) {
            Module._dataops();
        });
    });
});
*/

int main(int argc, char **argv)
{
    printf("Native: init main\n");

    const char *data[] = {
        "globals.js",
        "data_MAIN_NATIVE_str.js",
        "posi_MAIN_NATIVE_str.js",
    };

    import_modules(data, ARRAY_SIZE_P(data));

    return 0;

}
