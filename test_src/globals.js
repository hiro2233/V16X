console.log("init globals external");

function stringToUint(str) {
    //var str = btoa(unescape(encodeURIComponent(str)));
    var charList = str.split('');
    var uintArray = [];
    for (var i = 0; i < charList.length; i++) {
        uintArray.push(charList[i].charCodeAt(0));
    }
    return new Uint8Array(uintArray);
}

function websock_response(text)
{
    //var data = new TextEncoder("utf-8").encode(text);
    var data = stringToUint(text);

    // Get data byte size, allocate memory on the heap, and get pointer
    var nDataBytes = data.length * data.BYTES_PER_ELEMENT;
    var dataPtr = Module._malloc(nDataBytes);

    // Copy data to the heap (directly accessed from Module.HEAPU8)
    var dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, nDataBytes);
    dataHeap.set(new Uint8Array(data.buffer));

    // Call function and get result
    Module._set_websock_response(dataHeap.byteOffset, text.length);

    // Free memory
    Module._free(dataHeap.byteOffset);
}

function stream_response(text)
{
    //var str = new TextEncoder("utf-8").encode(text);
    var data = stringToUint(text);

    // Get data byte size, allocate memory on heap, and get pointer
    var nDataBytes = data.length * data.BYTES_PER_ELEMENT;
    var dataPtr = Module._malloc(nDataBytes);

    // Copy data to the heap (directly accessed from Module.HEAPU8)
    var dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, nDataBytes);
    dataHeap.set(new Uint8Array(data.buffer));

    // Call function and get result
    Module._set_stream_response(dataHeap.byteOffset, text.length);
    var result = new Float32Array(dataHeap.buffer, dataHeap.byteOffset, data.length);

    // Free memory
    Module._free(dataHeap.byteOffset);
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
    initialize: (function()
    {
        console.log("global test override ok");
        console_printf = Module.cwrap('console_printf', null, ['string']);
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
    Sum:({}),
    Sum_ccall:({}),
    set_cube_angle:({}),
    stop_drawing:({}),
    initialized: false,
    take_args: (function()
    {
        take_args(33, 44);
    }),
    draw_cube:({}),
    set_websock_response:({}),
    set_stream_response:({}),
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
};

globals.initialize();
//var result = globals.Sum_ccall(10, 12);
//console_printf("Sum_ccall: " + result);
document.getElementById('status').innerHTML = "globals init";
