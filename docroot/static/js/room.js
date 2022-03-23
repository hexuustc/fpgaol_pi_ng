// version = 1.6
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

var hw_init_json = JSON.stringify({
	'id': -2,
	'periphs': [{
			'type': 'LED',
			'idx' : 0,
		}, {
			'type': 'LED',
			'idx' : 1,
		}, {
			'type': 'BTN',
			'idx' : 0,
		}, {
			'type': 'BTN',
			'idx' : 1,
		}], 
});

var term_banner = '\x1B[1;1mFPGAOL \x1B[1;1;36mUART \x1B[1;1;32mxterm\x1B[1;1;33m.\x1B[1;1;31mjs \x1B[1;1;35m1.1\x1B[0m\r\n';

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
    // if (DEBUG_MODE) {
    //     uartSocket = new WebSocket('ws://' + DEBUG_WS_SERVER + '/uartws/');
    // } else {
    //     uartSocket = new WebSocket('ws://' + PI_SERVER_ADDRESS + '/uartws/');
    // }
    // uartSocket.onmessage = function (e) {
    //     var data = JSON.parse(e.data);
    //     // console.log(data['msg']);
    //     term.echo(data['msg']);
    // };
    // setup websocket
    if (DEBUG_MODE) {
        notifySocket = new WebSocket(
            'ws://' + DEBUG_WS_SERVER + '/ws/');
    } else {
        notifySocket = new WebSocket(
            'ws://' + PI_SERVER_ADDRESS + '/ws/');
    }
    notifySocket.onmessage = function (e) {
        console.log("receive message");
        var data = JSON.parse(e.data);
        var type = data['type'];
        var idx = data['idx'];
		var payload = data['payload'];
		if (type == 'LED') {
			setLed(idx, payload);
			var p = periphs[i];
			console.log(p.type);
		//} else if (type == 'BTN') {
			//setSeg(values);
		} else if (type == 'WF') {
			document.getElementById("download").innerHTML = "Download";
			$("#download").attr("href", "./waveform.vcd");
		} else if (type == 'XDC') {
			$("#copy-xdc").prop({'value': payload});
		}
			//term.write(values);
			//console.log("term output");
		//console.log("echo end");
    };

    // Waveform generation
    $("#generate").click(function () {
        $("#generate").attr("disabled", "true");
        document.getElementById("download").innerHTML = "Waiting...";
		sendStopNotify();
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
	$("#submit-json").click(function () {
		clear_periph();
		prepare_periph();
	});
	$("#copy-xdc").click(function () {
	});

    //for (var i = 0; i < 8; ++i) {
        //$("#sw" + i).change(function () {
            //for (var i = 0; i < 8; ++i) {
                //sendGpio(i, $("#sw" + i).prop("checked"));
            //}
        //});
    //}

    //// soft clock
    //$(".dropdown-item0").click(function (e) {
        //var v = e.currentTarget.getAttribute('val');
        //var t = e.currentTarget.getAttribute('text');
        //$("#clock0dropdownMenuButton").text(t);
        //sendGpio(-3, v);
    //});

    //// fpgabtn
    //$("#fpgabtn").mouseup(function (e) {
        //sendGpio(8, false);
    //});
    //$("#fpgabtn").mousedown(function (e) {
        //sendGpio(8, true);
    //});

    // Handle file upload.
    var progress = $("#progress");
    var filestatus = $("#filestatus");
    var statustext = $("#responsetext");

    $("#upload-button").click(function () {
        sendStopNotify();

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
						sendStartNotify(hw_init_json);
                    } else {
                        $("#upload-button").removeAttr("disabled");
                        filestatus.removeClass("alert-success");
                        filestatus.removeClass("alert-info");
                        filestatus.addClass("alert-danger");
                    }
                }
            };
            if (DEBUG_MODE) {
                xhr.open("POST", 'http://' + DEBUG_HTTP_SERVER + '/upload/?token=token_debug_ignore');
            } else {
                xhr.open("POST", '/upload/?token=' + token);
            }
            xhr.send(fd);
        });
    });

	clear_everything();

	//document.getElementById("jsoninput").value = hw_init_json;
	$("#jsoninput").prop({'value': hw_init_json});
	clear_periph();
	prepare_periph();

	// new xterm.js terminal
	//term = new Terminal();
	//term.open(document.getElementById('xterm'));
	//term.write(term_banner);
	//term.onData((val) => {
		////term.write(val);
		////sendGpio(-3, val);
	//})
	// start data acquisition after 1s, before web programming,
	// because programming may has been done in Vivado XVC
	setTimeout(function(){
		sendStopNotify();
		//clear_everything();
		sendStartNotify(hw_init_json);
	}, 1000);
});

function setLED(idx, val) {
	$("#led" + idx).prop({'checked': val ? true : false});
}
function SW_change(idx, val) {
	console.log("SW", idx, val);
	sendJson(JSON.stringify({
		'type': 'BTN',
		'idx': idx,
		'payload': val
	}));
}

//function setLed(values) {
    //for (var i = 0; i < 8; ++i) {
        //$(".segplay:eq(" + i + ")").attr("value", Math.floor(values[i] % 10) ? "1" : "0");
        //$("#led" + i).prop({ 'checked': Math.floor(values[i] % 10) ? true : false });
    //}
//}

//function setSeg(values) {
    //for (var i = 0; i < 8; ++i) {
        //$("#hexplay_span" + i).html(values[i] == -1 ? '&nbsp;' : ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'b', 'C', 'd', 'E', 'F'][values[i]]);
    //}
//}

function sendStopNotify() {
    notifySocket.send(JSON.stringify({
        'id': -1,
    }));
}

function sendStartNotify(json) {
	var data = JSON.parse(json);
	var id = data['id'];
	if (id == -2) {
		notifySocket.send(json);
	}
	else {
		console.log("sendStartNotify: wrong id!");
	}
}

// parse json on panel, initialize frontend peripherals
function prepare_periph() {
	var json = document.getElementById("jsoninput").value;
	var data = JSON.parse(json);
	var periphs = data['periphs'];
	for(var i = 0; i < periphs.length; i++) {
		var p = periphs[i];
		console.log(p.type, p.idx);
		if (p.type == 'LED') {
			//
		} else if (p.type == 'BTN') {
			$('#sw' + p.idx).prop("disabled", false);
			$('#sw' + p.idx).change(function() {
				SW_change(this.id.slice(2), this.checked);
			});
		}
	}

}

function clear_periph() {
	for (var i = 0; i < 16; ++i) {
		$("#sw" + i).prop({'checked': false, 'disabled': true});
		$("#sw" + i).off('change');
		//$("#sw" + i).hide();
		$("#led" + i).prop({'checked': false});
		$("#led" + i).off('change');
		//$("#led" + i).addClass('d-none');
	}
}

function sendJson(json) {
	console.log(json);
	notifySocket.send(json);
}

function clear_everything() {
	clear_periph();
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
