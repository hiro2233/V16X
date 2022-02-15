console.log("init globals external");

function DataToUint(str)
{
    var charList = str;
    var uintArray = [];
    for (var i = 0; i < charList.length; i++) {
        uintArray.push(charList[i].charCodeAt(0));
    }
    return new Uint8Array(uintArray);
}

function stringToUint(str)
{
    //var str = btoa(unescape(encodeURIComponent(str)));
    var charList = str.split('');
    var uintArray = [];
    for (var i = 0; i < str.length; i++) {
        uintArray.push(charList[i].charCodeAt(0));
    }
    return new Uint8Array(uintArray);
}

function websock_response(text, length)
{
    //var data = new TextEncoder("utf-8").encode(text);
    //var data = DataToUint(text);
    /*
        var data = text;

        // Get data byte size, allocate memory on the heap, and get pointer
        var nDataBytes = data.length * data.BYTES_PER_ELEMENT;
        var dataPtr = Module._malloc(nDataBytes);

        // Copy data to the heap (directly accessed from Module.HEAPU8)
        var dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, nDataBytes);
        dataHeap.set(new Uint8Array(data.buffer));
    */
    Module.ccall('set_websock_response', 'v', ['array', 'number'], [text, text.length]);
    // Call function and get result
    //Module._set_websock_response(dataHeap.byteOffset, text.length);

    // Free memory
    //Module._free(dataHeap.byteOffset);
}

function stream_response(text, length)
{
    /*
        //var str = new TextEncoder("utf-8").encode(text);
        var data = stringToUint(text);

        // Get data byte size, allocate memory on heap, and get pointer
        var nDataBytes = text.length * data.BYTES_PER_ELEMENT;
        var dataPtr = Module._malloc(nDataBytes);

        // Copy data to the heap (directly accessed from Module.HEAPU8)
        var dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, nDataBytes);
        dataHeap.set(new Uint8Array(data.buffer));
    */
    //var ppcStr;
    var pcntstr;
    //var res1 = Module._malloc(4);

    // Call function and get result
    //var res = Module._set_stream_response(dataHeap.byteOffset, text.length, pcntstr);
    var streamres = Module.ccall('set_stream_response', 'number', ['string', 'number', 'number'],[text, text.length, pcntstr]);
    //streamres = Module.cwrap('set_stream_response', 'number', ['string','number','number']);
    //var res1 = streamres(text, text.length, pcntstr);
    var res = streamres;

    var cntstr = Module.HEAP32[pcntstr >> 2];
    //var arr = res;
    var arr = [];
    for (var i = 0; i < cntstr; i++) {
        arr.push(Module.UTF8ToString(Module.HEAP32[(res >> 2) + i]));
    }

    console.log("STREAM RESPONSE: ", arr, cntstr);
    // Free memory
    //Module._free(dataHeap.byteOffset);
    return arr;
}

var globals_callbacks = typeof globals_callbacks !== 'undefined' ? globals_callbacks : {};

function connect_to_deepservice(ip, port)
{
    //var str = new TextEncoder("utf-8").encode(text);
    var data = stringToUint(ip);

    // Get data byte size, allocate memory on the heap, and get pointer
    var nDataBytes = data.length * data.BYTES_PER_ELEMENT;
    var dataPtr = Module._malloc(nDataBytes);

    // Copy data to the heap (directly accessed from Module.HEAPU8)
    var dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, nDataBytes);
    dataHeap.set(new Uint8Array(data.buffer));

    // Call function and get result
    Module._connect_to_deepservice(dataHeap.byteOffset, ip.length, port);
    var result = new Float32Array(dataHeap.buffer, dataHeap.byteOffset, data.length);

    // Free memory
    Module._free(dataHeap.byteOffset);
}

const globals = {
initialize:
    (function()
    {
        console.log("global test override ok");
        this.console_printf = Module.cwrap('console_printf', null, ['string']);
        //console_printf = Module['_console_printf'];
        this.get_counter = Module['_get_counter'];
        this.set_cube_angle = Module['_set_cube_angle'];
        this.Sum = Module['_Sum'];
        this.Sum_ccall = Module['_Sum_ccall'];
        this.draw_cube = Module['_draw_cube'];
        this.stop_drawing = Module['_stop_drawing'];
        this.send_deepservice_ping = Module['_send_deepservice_ping'];

        this.set_websock_response = websock_response;
        this.set_stream_response = stream_response;
        this.connect_to_deepservice = connect_to_deepservice;

        this.on_initialize();

        this.initialized = true;
    }),
    Sum: ({}),
    Sum_ccall: ({}),
    set_cube_angle: ({}),
    stop_drawing: ({}),
    initialized: false,
    take_args: (function()
    {
        take_args(33, 44);
    }),
    draw_cube: ({}),
    set_websock_response: ({}),
    set_stream_response: ({}),
    on_initialize: (function()
    {
        globals_callbacks.oninit();
    }),
    connect_to_deepservice: ({}),
    send_deepservice_ping: ({}),
    on_send_sds_failed: (function()
    {
        globals_callbacks.on_send_sds_failed();
    }),
    get_counter: ({}),
};

var unboundSlice = Array.prototype.slice;
var _slice = Function.prototype.call.bind(unboundSlice);

if (!Uint8Array.prototype.slice)
{
    Object.defineProperty(Uint8Array.prototype, 'slice', {
    value: function (begin, end)
        {
            return new Uint8Array(_slice(this, begin, end));
        }
    });
}


Number.prototype.pad = function(size)
{
    var s = String(this);
    while (s.length < (size || 2)) {
        s = "0" + s;
    }
    return s;
}

if (!String.prototype.padStart)
{
    String.prototype.padStart = function padStart(targetLength,padString) {
        targetLength = targetLength>>0; //truncate if number or convert non-number to 0;
        padString = String((typeof padString !== 'undefined' ? padString : ' '));
        if (this.length > targetLength) {
            return String(this);
        } else {
            targetLength = targetLength-this.length;
            if (targetLength > padString.length) {
                padString += padString.repeat(targetLength/padString.length); //append to original to ensure we are longer than needed
            }
            return padString.slice(0,targetLength) + String(this);
        }
    };
}

/*
function slice() {
  return _slice(arguments, 0);
}
*/
/*
if (!Uint8Array.prototype.slice) {
  Object.defineProperty(Uint8Array.prototype, 'slice', {
    value: function (begin, end)
     {
        return new Uint8Array(Array.prototype.slice.call(this, begin, end));
     }
  });
}

if (!Uint16Array.prototype.slice) {
  Object.defineProperty(Uint16Array.prototype, 'slice', {
    value: function (begin, end)
     {
        return new Uint16Array(Array.prototype.slice.call(this, begin, end));
     }
  });
}
*/
function UR_DataObj()
{
    this.typeVal = {
    uint8:
        {
            val: 1,
    tArray:
            Uint8Array,
        },
    uint16:
        {
            val: 2,
    tArray:
            Uint16Array,
        },
    uint32:
        {
            val: 4,
    tArray:
            Uint32Array,
        },
    }

    function constructor() {
    }

    this.get_array = function (vectemp) {
        //console.log("\n[ Init Getter ] -----");
        var size_array = 0;

        for (var key in vectemp) {
            if (vectemp.hasOwnProperty(key)) {
                size_array += this.typeVal[vectemp[key].type].val;
                //console.log(key + "=" + vectemp[key].val + " Typeval: " + this.typeVal[vectemp[key].type].val + " TypeArray: " + this.typeVal[vectemp[key].type].tArray);
            }
        }

        var posbuf = 0;
        var lastpos = 0;
        var buftmp = new ArrayBuffer(size_array);
        var buftmpview = new Uint8Array(buftmp);

        //console.log("\n");

        if (typeof globals != 'undefined') {
            globals.console_printf("........");
        }

        for (var key in vectemp) {
            if (vectemp.hasOwnProperty(key)) {
                var tarray = this.typeVal[vectemp[key].type].val + posbuf;

                var buftmp1 = new ArrayBuffer(this.typeVal[vectemp[key].type].val);
                var buftmp1view = new Uint8Array(buftmp1);
                var datatmp = new this.typeVal[vectemp[key].type].tArray(buftmp1view.buffer);

                datatmp[0] = vectemp[key].val;

                buftmpview.set(buftmp1view, posbuf);

                posbuf += this.typeVal[vectemp[key].type].val;
                lastpos = this.typeVal[vectemp[key].type].val;

                //datatmp[0] = vectemp[key].val;

                //console.log(key +" view: " + datatmp[0]);
                var msgdata = "GET s[0]: " + datatmp[0];

                if (typeof globals != 'undefined') {
                    globals.console_printf(msgdata);
                }
            }
        }

        if (typeof globals != 'undefined') {
            globals.console_printf(".........");
        }

        //console.log("\nsize_array: " + size_array);
        //console.log("Posbuf: " + (posbuf - lastpos));
        //console.log("[ End  Getter ]  -----\n");

        var bytearray = new Uint8Array(buftmpview.slice());
        var ret_array = new Uint8Array(bytearray.buffer);

        //console.log("ret_array: " + ret_array);

        return ret_array;
    }

    this.set_array = function (vectemp, hexstr) {
        console.log("\n[ Init Setter ] -----");
        var size_array = 0;

        for (var key in vectemp) {
            if (vectemp.hasOwnProperty(key)) {
                size_array += this.typeVal[vectemp[key].type].val;
                //console.log(key + "=" + vectemp[key].val + " Typeval: " + this.typeVal[vectemp[key].type].val + " TypeArray: " + this.typeVal[vectemp[key].type].tArray);
            }
        }

        var msgdata = "size array: " + size_array;
        if (typeof globals != 'undefined') {
            globals.console_printf(msgdata);
        }

        var posbuf = 0;
        var lastpos = 0;
        //var buftest1 = this.str2bin(hexstr);
        //const buftmp = new Uint8Array(buftest1);

        //const buftmp = new ArrayBuffer(size_array);
        //const view = new Uint8Array(buftmp);

        //var strtmp = DataToUint(hexstr);
        var strtmp = hexstr;

        msgdata = "s[0]: " + strtmp[0] + " s[1]: " + strtmp[1] + " s[2]: " + strtmp[2] + " s[3]: " + strtmp[3];

        if (typeof globals != 'undefined') {
            globals.console_printf(msgdata);
        }

        var tdata = new this.typeVal[vectemp["data1"].type].tArray(strtmp.buffer);
        var string = new TextDecoder().decode(tdata);
        msgdata = "typeVal: " + string;

        if (typeof globals != 'undefined') {
            globals.console_printf(msgdata);
        }

        for (var key in vectemp) {
            if (vectemp.hasOwnProperty(key)) {
                //let datatmp1 = new this.typeArray[this.typeVal[vectemp[key].type].tArray](buftmp.buffer, posbuf, this.typeVal[vectemp[key].type].val);
                var tarray = this.typeVal[vectemp[key].type].val + posbuf;

                var buftmp = new ArrayBuffer(size_array);
                var view = new Uint8Array(buftmp);

                view.set(strtmp.slice(posbuf, tarray));

                var sliced = new this.typeVal[vectemp[key].type].tArray(view.buffer);
                msgdata = "sliced[0]: " + sliced[0];

                if (typeof globals != 'undefined') {
                    globals.console_printf(msgdata);
                }

                posbuf += this.typeVal[vectemp[key].type].val;
                lastpos = this.typeVal[vectemp[key].type].val;
                vectemp[key].val = sliced[0];

                //console.log(vectemp);
                //console.log(key +" view: " + datatmp[0]);
                //console.log(key + " posbuf: " + posbuf + " lastpos: " + lastpos);
            }
        }

        //console.log("\nsize_array: " + size_array);
        //console.log("Posbuf: " + (posbuf - lastpos));
        //console.log("[ End  Setter ]  -----\n");

    }

    this.strhex2bin = function (hex) {
        for (var bytes = [], c = 0; c < hex.length; c += 2)
            bytes.push(parseInt(hex.substr(c, 2), 16));
        return bytes;
    }

    this.bin2strhex = function (str) {
        var strarray = [];
        for (var i = 0; i < str.byteLength; i++) {
            strarray[i] = Number(str[i]).toString(16).padStart(2, '0');
        }
        return strarray.join('');
    }

}

//globalThis.UR_DataObj = UR_DataObj;

globals.initialize();
//var result = globals.Sum_ccall(10, 12);
//console_printf("Sum_ccall: " + result);
document.getElementById('status').innerHTML = "globals init";
