// version = 1.1
// global variables
// FPGAOL_NG_DEV
var DEBUG_MODE = false;
var DEBUG_HTTP_SERVER = "127.0.0.1:8080";
var DEBUG_WS_SERVER = "127.0.0.1:8090";

var token;
var term;
var notifySocket;
var PI_SERVER_ADDRESS;

var hexPlayDigits = [];

function GetRequest() {
    var url = location.search;
    var theRequest = new Object();
    if (url.indexOf("?") != -1) {
        var str = url.substr(1);
        strs = str.split("&");
        for (var i = 0; i < strs.length; i++) {
            theRequest[strs[i].split("=")[0]] = decodeURI(strs[i].split("=")[1]);
        }
    }
    return theRequest;
}

$(document).ready(function () {
    token = GetRequest()['token'];

    // waveform
    $("#view-button").click(function () {
        viewPeriod();
    });

    PI_SERVER_ADDRESS = window.location.host;
    // uart websocket
    if (DEBUG_MODE) {
        uartSocket = new WebSocket('ws://' + DEBUG_WS_SERVER + '/uartws/');
    } else {
        uartSocket = new WebSocket('ws://' + PI_SERVER_ADDRESS + '/uartws/');
    }
    uartSocket.onmessage = function (e) {
        var data = JSON.parse(e.data);
        // console.log(data['msg']);
        term.echo(data['msg']);
    };
    // setup websocket
    if (DEBUG_MODE) {
        notifySocket = new WebSocket(
            'ws://' + DEBUG_WS_SERVER + '/ws/');
    } else {
        notifySocket = new WebSocket(
            'ws://' + PI_SERVER_ADDRESS + '/ws/');
    }
    notifySocket.onmessage = function (e) {
        var data = JSON.parse(e.data);
        var type = data['type'];
        var values = data['values'];
        if (type == 'LED') {
            setLed(values);
        } else if (type == 'SEG') {
            setSeg(values);
        } else if (type == 'WF') {
            document.getElementById("download").innerHTML = "Download";
            $("#download").attr("href", "./waveform.vcd");
        }
    };

    // Waveform generation
    $("#generate").click(function () {
        $("#generate").attr("disabled", "true");
        document.getElementById("download").innerHTML = "Waiting...";
        sendGpio(-1, false);
    });

    notifySocket.onclose = function () {
        $("#timeoutModal").modal("show");
    };

    // set ui event functions
    $("#file-select").click(function () {
        document.getElementById("bitstream").click();
    });
    $("#bitstream").change(function () {
        $("#file-name").val($("#bitstream").val());
    });

    for (var i = 0; i < 8; ++i) {
        $("#sw" + i).change(function () {
            for (var i = 0; i < 8; ++i) {
                sendGpio(i, $("#sw" + i).prop("checked"));
            }
        });
    }

    // soft clock
    $(".dropdown-item0").click(function (e) {
        var v = e.currentTarget.getAttribute('val');
        var t = e.currentTarget.getAttribute('text');
        $("#clock0dropdownMenuButton").text(t);
        sendGpio(-3, v);
    });

    // fpgabtn
    $("#fpgabtn").mouseup(function (e) {
        sendGpio(8, false);
    });
    $("#fpgabtn").mousedown(function (e) {
        sendGpio(8, true);
    });

    // Handle file upload.
    var progress = $("#progress");
    var filestatus = $("#filestatus");
    var statustext = $("#responsetext");
    $("#upload-button").click(function () {
        sendGpio(-1, true); // Stop notification

        var file = $("#bitstream")[0].files[0];
        var zip = new JSZip();
        zip.file("bitstream.bit", file, { binary: true, unixPermissions: "755" });
        zip.generateAsync({ type: "blob", compression: "DEFLATE" }).then(function (zipped_file) {
            var fd = new FormData();
            fd.append('bitstream', zipped_file);
            fd.append('judge', 'normal');
            var xhr = new XMLHttpRequest();
            xhr.upload.onprogress = function (e) {
                $("#upload-button").attr("disabled", "false");
                filestatus.removeClass("alert-danger");
                filestatus.removeClass("alert-success");
                filestatus.addClass("alert-info");
                progress.addClass("progress-bar-animated");
                if (e.lengthComputable) {
                    progress.attr("style", "width:" + e.loaded / e.total * 100 + "%");
                    statustext.text("Uploading bitstream file");
                    if (e.loaded >= e.total) {
                        statustext.text("Programming device");
                    }
                }
            };

            xhr.onreadystatechange = function () {
                if (this.readyState == 3) {
                    statustext.text("Programming device, please wait");
                } else if (this.readyState == 4) {
                    progress.removeClass("progress-bar-animated");
                    statustext.text(this.responseText);
                    if (this.status == 200) {
                        clear_everything();
                        filestatus.removeClass("alert-danger");
                        filestatus.removeClass("alert-info");
                        filestatus.addClass("alert-success");
                        sendGpio(-2, false);
                    } else {
                        $("#upload-button").removeAttr("disabled");
                        filestatus.removeClass("alert-success");
                        filestatus.removeClass("alert-info");
                        filestatus.addClass("alert-danger");
                    }
                }
            };
            if (DEBUG_MODE) {
                xhr.open("POST", 'http://' + DEBUG_HTTP_SERVER + '/upload/');
            } else {
                xhr.open("POST", '/upload/?token=' + token);
            }
            xhr.send(fd);
        });
    });
    // uart terminal
    term = $('#terminal').terminal(function (command) {
        uartSocket.send(JSON.stringify({ 'msg': command }));
    }, { prompt: '>', name: 'test', greetings: 'FPGAOL uart beta 1.0' });

});

function setLed(values) {
    for (var i = 0; i < 8; ++i) {
        $(".segplay:eq(" + i + ")").attr("value", Math.floor(values[i] % 10) ? "1" : "0");
        $("#led" + i).prop({ 'checked': Math.floor(values[i] % 10) ? true : false });
    }
}

function setSeg(values) {
    for (var i = 0; i < 8; ++i) {
        $("#hexplay_span" + i).html(values[i] == -1 ? '&nbsp;' : ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'b', 'C', 'd', 'E', 'F'][values[i]]);
    }
}

function sendGpio(gpio, level) {
    notifySocket.send(JSON.stringify({
        'gpio': gpio,
        'level': level,
    }));
}

function clear_everything() {
    for (var i = 0; i < 8; ++i) {
        $("#sw" + i).prop({ 'checked': false });
    }

    setSeg([-1, -1, -1, -1, -1, -1, -1, -1]);
    setLed([0, 0, 0, 0, 0, 0, 0, 0]);

    $("#upload-button").removeAttr("disabled");
    $("#generate").removeAttr("disabled");
    document.getElementById("download").innerHTML = "";
}

/*
 * debug functions
 *
 * $('#fpgaol-message-submit').click(function (e) {
 *     var gpioDom = document.querySelector('#gpio');
 *     var levelDom = document.querySelector('#level');
 *     var gpio = gpioDom.value;
 *     var level = levelDom.value;
 *     sendGpio(gpio, level);
 * });
 *
 * $('#stop-notify').click(function (e) {
 *     var message = 'Stop notifications';
 *     document.querySelector('#cmd-log').value += (message + '\n');
 *     notifySocket.send(JSON.stringify({
 *         'gpio': -1,
 *         'level': 0,
 *     }));
 * });
 */
