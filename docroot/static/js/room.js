// version = 1.1
// global variables
// FPGAOL_NG_DEV
var DEBUG_MODE = true;
var DEBUG_HTTP_SERVER = "127.0.0.1:8080";
var DEBUG_WS_SERVER = "127.0.0.1:8090";
var globalTime;
var myChart;
var term;
var notifySocket;
var waveformmode = 'last';
var notifyRecord = [];
var series = [];
var leds = [0, 0, 0, 0, 0, 0, 0, 0];
var segs = [0, 0, 0, 0, 0, 0];
var xAxis = {
    type: 'value',
    min: 0,
    max: 10
};
var PI_SERVER_ADDRESS;

function initSeries() {
    for (var i = 0; i < 8; ++i) {
        series[i] = {
            name: 'Led' + i,
            type: 'line',
            step: 'start',
            data: [[0, 'led' + i + ':0']]
        };
    }
}
initSeries();
// config json
var option = {
    title: {
        text: 'waveform'
    },
    /*tooltip: {
        trigger: 'axis'
    },*/
    grid: {
        left: '3%',
        right: '4%',
        bottom: '3%',
        containLabel: true
    },
    toolbox: {
        feature: {
            saveAsImage: {}
        }
    },
    xAxis: {
        type: 'value',
        min: 0,
        max: 10
    },
    yAxis: {
        type: 'category',
        data: ['led0:0', 'led0:1', 'led1:0', 'led1:1', 'led2:0', 'led2:1', 'led3:0', 'led3:1',
            'led4:0', 'led4:1', 'led5:0', 'led5:1', 'led6:0', 'led6:1', 'led7:0', 'led7:1'
        ]
    }
};
$(window).resize(function () {
    myChart.resize();
});


$(document).ready(function () {
    // waveform
    var app = {};
    var chartContainer = document.getElementById("chart-container");
    myChart = echarts.init(chartContainer);
    myChart.setOption(option, true);

    $("#view-button").click(function () {
        viewPeriod();
    });

    $("#inlineRadio1").change(function () {
        waveformmode = $("input:radio[name=\"inlineRadioOptions\"]:checked").val();
    });
    $("#inlineRadio2").change(function () {
        waveformmode = $("input:radio[name=\"inlineRadioOptions\"]:checked").val();
    });
    PI_SERVER_ADDRESS = window.location.host + ':' + '100' + $("#pi").text().substr(2);
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
        var gpio = parseInt(data['gpio']);
        var level = parseInt(data['level']);
        var tick = parseInt(data['tick']);
        if (gpio == -1) {
            var lvals = level;
            globalTime = tick / 1000000;
            for (var i = 0; i < 8; ++i) {
                var lval = lvals % 2;
                setLed(i, lval);
                lvals >>= 1;
                var record = [i, lval, tick / 1000000];
                notifyRecord.push(record);
                chartAppendData(record, i == 7);
            }
            for (var i = 0; i < 6; ++i) {
                var lval = lvals % 2;
                setSeg(i, lval);
                lvals >>= 1;
            }
            timeShift();
        } else if (gpio <= 7) {
            setLed(gpio, level);
            var record = [gpio, level, tick / 1000000];
            notifyRecord.push(record);
            chartAppendData(record, false);
        } else {
            setSeg(gpio - 8, level);
        }
    };

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
        sendGpio(8, 0);
    });
    $("#fpgabtn").mousedown(function (e) {
        sendGpio(8, 1);
    });

    // Handle file upload.
    var progress = $("#progress");
    var filestatus = $("#filestatus");
    var statustext = $("#responsetext");
    $("#upload-button").click(function () {
        var fd = new FormData();
        fd.append("bitstream", $("#bitstream")[0].files[0]);
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
                    $("#upload-button").removeAttr("disabled");
                    notifyRecord = [];
                    notifyRecord.length = 0;
                    xAxis = {
                        type: 'value',
                        min: 0,
                        max: 10
                    };
                    Series = [];
                    initSeries();
                    myChart.clear();
                    myChart.setOption(option, true);
                    filestatus.removeClass("alert-danger");
                    filestatus.removeClass("alert-info");
                    filestatus.addClass("alert-success");
                    globalTime = 0;
                    sendGpio(-2, 0);
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
            xhr.open("POST", 'http://' + PI_SERVER_ADDRESS + '/upload/');
        }
        xhr.send(fd);
    });

    // uart terminal
    term = $('#terminal').terminal(function (command) {
        uartSocket.send(JSON.stringify({ 'msg': command }));
    }, { prompt: '>', name: 'test', greetings: 'FPGAOL uart beta 1.0' });
});

function setLed(ledid, ledstatus) {
    leds[ledid] = ledstatus % 10;
    $(".segplay:eq(" + ledid + ")").attr("value", Math.floor(ledstatus % 10) ? "1" : "0");
    $("#led" + ledid).prop({ 'checked': Math.floor(ledstatus % 10) ? true : false });
}

function setSeg(segid, segstatus) {
    segs[segid] = segstatus % 10;
    setHexplay();
}

var hexPlaySegs = [["1", "1", "1", "1", "1", "1", "0", "0"], ["0", "1", "1", "0", "0", "0", "0", "0"], ["1", "1", "0", "1", "1", "0", "1", "0"], ["1", "1", "1", "1", "0", "0", "1", "0"], ["0", "1", "1", "0", "0", "1", "1", "0"], ["1", "0", "1", "1", "0", "1", "1", "0"], ["1", "0", "1", "1", "1", "1", "1", "0"], ["1", "1", "1", "0", "0", "0", "0", "0"], ["1", "1", "1", "1", "1", "1", "1", "0"], ["1", "1", "1", "1", "0", "1", "1", "0"], ["1", "1", "1", "0", "1", "1", "1", "0"], ["0", "0", "1", "1", "1", "1", "1", "0"], ["0", "0", "0", "1", "1", "0", "1", "0"], ["0", "1", "1", "1", "1", "0", "1", "0"], ["1", "0", "0", "1", "1", "1", "1", "0"], ["1", "0", "0", "0", "1", "1", "1", "0"]];

function setHexplay() {
    // 0 1 2 3 4 5 6 7 8 9 A b c d E F
    var v = segs[3] * 8 + segs[2] * 4 + segs[1] * 2 + segs[0];
    var s = hexPlaySegs[v];
    var select = "#hex" + (3 - (segs[5] * 2 + segs[4]));
    for (var i = 0; i < 7; ++i) {
        $(select + i).attr("value", s[i]);
    }
    /*     for (var j = 0; j < 4; ++j) {
            if (segs[7 - j] == 0) {
                for (var i = 0; i < 7; ++i) {
                    $("#hex" + j + i).attr("value", "0");
                }
            }
        } */
}

function sendGpio(gpio, level) {
    notifySocket.send(JSON.stringify({
        'gpio': gpio,
        'level': level,
    }));
}

function viewPeriod() {
    if (waveformmode == 'period') {
        var start = $("#start-time").val();
        var finish = $("#finish-time").val();
        var periodSeries = [];
        var periodxAxis = {
            type: 'value',
            min: start,
            max: finish
        };
        for (var i = 0; i < 8; ++i) {
            periodSeries[i] = {
                name: 'Led' + i,
                type: 'line',
                step: 'start',
                data: []
            };
        }
        for (var i = 0; i < notifyRecord.length; ++i) {
            if (notifyRecord[i][2] < start) {
                continue;
            } else if (notifyRecord[i][2] > finish) {
                break;
            } else {
                var gpio = notifyRecord[i][0];
                periodSeries[gpio].data.push([notifyRecord[i][2],
                'led' + gpio + ':' + notifyRecord[i][1]]);
            }
        }
        // console.log(periodSeries);
        myChart.setOption({
            series: periodSeries,
            xAxis: periodxAxis
        });
    }
}

function updateChartXaxis() {
    if (waveformmode == 'last') {
        myChart.setOption({
            xAxis: xAxis
        });
    }
}

function updateChartSeries() {
    if (waveformmode == 'last') {
        myChart.setOption({
            series: series
        });
    }
}

function timeShift() {
    if (xAxis.max < globalTime) {
        xAxis.max += 1;
        xAxis.min += 1;
        for (var i = 0; i < 8; ++i) {
            while (series[i].data[0][0] < xAxis.min - 2) {
                series[i].data.shift();
            }
        }
        updateChartXaxis();
    }
}

function chartAppendData(data, update = true) {
    var i = data[0];
    series[i].data.push([data[2], 'led' + i + ':' + data[1]]);
    if (update) {
        updateChartSeries();
    }
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
