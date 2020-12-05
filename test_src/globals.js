console.log("init globals external");

function stringToUint(str) {
    //var str = btoa(unescape(encodeURIComponent(str))),
    charList = str.split(''),
    uintArray = [];
    for (var i = 0; i < charList.length; i++) {
        uintArray.push(charList[i].charCodeAt(0));
    }
    return new Uint8Array(uintArray);
}

function websock_response(text, len)
{
    //var data = new TextEncoder("utf-8").encode(text);
    var data = stringToUint(text);

    // Get data byte size, allocate memory on Emscripten heap, and get pointer
    var nDataBytes = data.length * data.BYTES_PER_ELEMENT;
    var dataPtr = Module._malloc(nDataBytes);

    // Copy data to Emscripten heap (directly accessed from Module.HEAPU8)
    var dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, nDataBytes);
    dataHeap.set(new Uint8Array(data.buffer));

    // Call function and get result
    Module._set_websock_response(dataHeap.byteOffset, len);

    // Free memory
    Module._free(dataHeap.byteOffset);
}

function stream_response(text, len)
{
    //var str = new TextEncoder("utf-8").encode(text);
    var data = stringToUint(text);

    // Get data byte size, allocate memory on Emscripten heap, and get pointer
    var nDataBytes = data.length * data.BYTES_PER_ELEMENT;
    var dataPtr = Module._malloc(nDataBytes);

    // Copy data to Emscripten heap (directly accessed from Module.HEAPU8)
    var dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, nDataBytes);
    dataHeap.set(new Uint8Array(data.buffer));

    // Call function and get result
    Module._set_stream_response(dataHeap.byteOffset, len);
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
        //this.set_websock_response = Module['_set_websock_response'];
        //this.set_websock_response = Module.cwrap('set_websock_response', null, ['string']);
        this.set_websock_response = websock_response;
        this.set_stream_response = stream_response;

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
    set_stream_response:({})
};

globals.initialize();
//var result = globals.Sum_ccall(10, 12);
//console_printf("Sum_ccall: " + result);
document.getElementById('status').innerHTML = "globals init";
