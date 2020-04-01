function iframeResize() {
    var wh = $(window).height();
    var offset = $("#helpdoc").offset();
    $("#helpdoc").height(wh - offset.top - 10);
}
$(document).ready(function () {
    iframeResize();
});
$(window).resize(function () {
    iframeResize();
});