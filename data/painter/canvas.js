var toolType = ["pen", "eraser", "rectangle", "ellipse"];

var canvasDownAction = {
    pen: penDown,
    eraser: eraserDown,
    rectangle: rectangleDown,
    ellipse: ellipseDown
};

var canvasUpAction = {
    pen: penUp,
    eraser: eraserUp,
    rectangle: rectangleUp,
    ellipse: ellipseUp
};

var canvasMoveAction = {
    pen: penMove,
    eraser: eraserMove,
    rectangle: rectangleMove,
    ellipse: ellipseMove
};

// HTML => Exe
function SendData(json)
{
    // window.cefQuery({
    //     request: json,
    //     onSuccess: function(response) {},
    //     onFailure: function(error_code, error_message) {}
    // });
}

var canvas;
var ctx;
var bufCanvas;
var bufCtx;
var remainCanvas;
var remainCtx;

let hueColorValue = 0;

var canvasHistory = {
    list: [],
    count: 0,

    clearHistory: function() {
        while (this.list.length) {
            this.list.pop();
        }
        this.count = 0;

        var aSendData = {};
        aSendData["action_type"] = "enable_undo";
        SendData(JSON.stringify(aSendData));
    },

    addHistory: function(history, addCount, redo) {
        if (addCount && this.count < 20) {
            this.count++;
        }

        var aSendData = {};
        aSendData["action_type"] = "able_undo";
        SendData(JSON.stringify(aSendData));

        if (20 == this.count && addCount && !redo) {
            var remainHistory = [];

            var firstHistory = this.list.shift();
            remainHistory.push(firstHistory);

            var firstHistoryArray = firstHistory.split(" ");

            if (firstHistoryArray[0].trim() == "pen_start") {
                while (this.list.length > 0) {
                    firstHistory = this.list.shift();
                    remainHistory.push(firstHistory);

                    firstHistoryArray = firstHistory.split(" ");
                    firstHistory = firstHistory.trim();
                    if (firstHistory.length == 0) {
                        continue;
                    }

                    if (firstHistoryArray[0].trim() == "pen_end") {
                        break;
                    }
                }
            }

            if (firstHistoryArray[0].trim() == "eraser_start") {
                while (this.list.length > 0) {
                    firstHistory = this.list.shift();
                    remainHistory.push(firstHistory);

                    firstHistoryArray = firstHistory.split(" ");
                    firstHistory = firstHistory.trim();
                    if (firstHistory.length == 0) {
                        continue;
                    }

                    if (firstHistoryArray[0].trim() == "eraser_end") {
                        break;
                    }
                }
            }

            bufCtx.clearRect(0, 0, canvas.width, canvas.height);

            if (typeof drawCanvasHistory2 === "function") {
                drawCanvasHistory2(remainCanvas, remainCtx, remainHistory);
            }
        }

        this.list.push(history);
    },

    removeHistroy: function(removeCount) {
        var aSendData = {};
        aSendData["action_type"] = "enable_undo";
        var jsonString = JSON.stringify(aSendData);

        if (removeCount && this.count > 0) {
            this.count--;
        }

        var history = this.list.pop();

        if (this.list.length < 1 || this.count < 1) {
            SendData(jsonString);
        }

        return history;
    }
};

var canvasRedoHistory = {
    list: [],
    count: 0,

    clearHistory: function() {
        while (this.list.length) {
            this.list.pop();
        }
        this.count = 0;

        var aSendData = {};
        aSendData["action_type"] = "enable_redo";
        SendData(JSON.stringify(aSendData));
    },

    addHistory: function(history, addCount) {
        if (addCount && this.count < 20) {
            this.count++;
        }

        var aSendData = {};
        aSendData["action_type"] = "able_redo";
        SendData(JSON.stringify(aSendData));

        if (20 == this.count && addCount) {
            var firstHistory = this.list.shift();

            if (firstHistory.trim() == "pen_end") {
                while (this.list.length > 0) {
                    firstHistory = this.list.shift();
                    firstHistory = firstHistory.trim();
                    if (firstHistory.length == 0) {
                        continue;
                    }

                    if (firstHistory.trim() == "pen_start") {
                        break;
                    }
                }
            }

            if (firstHistory.trim() == "eraser_end") {
                while (this.list.length > 0) {
                    firstHistory = this.list.shift();
                    firstHistory = firstHistory.trim();
                    if (firstHistory.length == 0) {
                        continue;
                    }

                    if (firstHistory.trim() == "eraser_start") {
                        break;
                    }
                }
            }
        }

        this.list.push(history);
    },

    removeHistroy: function(removeCount) {
        var aSendData = {};
        aSendData["action_type"] = "enable_redo";
        var jsonString = JSON.stringify(aSendData);

        if (removeCount && this.count > 0) {
            this.count--;
        }

        var history = this.list.pop();

        if (this.list.length < 1 && this.count < 1) {
            SendData(jsonString);
        }

        return history;
    }
};

var painter = {
    isDown: false,
    color: "rgb(255,36,36)",
    alpha: 1,
    bgR: 255,
    bgG: 255,
    bgB: 255,
    tool: 0,
    thickness: 4,
    x: 0,
    y: 0,
    isPrevUndo: false,

    mouseDown: canvasDownAction[toolType[0]],
    mouseUp: canvasUpAction[toolType[0]],
    mouseMove: canvasMoveAction[toolType[0]],

    updateTool: function(tool) {
        this.tool = tool;
        this.mouseDown = canvasDownAction[toolType[tool]];
        this.mouseUp = canvasUpAction[toolType[tool]];
        this.mouseMove = canvasMoveAction[toolType[tool]];
    },

    updateRGB: function(r, g, b) {
        this.bgR = r;
        this.bgG = g;
        this.bgB = b;
    },

    updateColor: function(color) {
        this.color = color;
    },

    updateThickness: function(thickness) {
        this.thickness = thickness;
    },

    updateAlpha: function(alpha) {
        this.alpha = alpha;
    }
};

// pen, eraser, rectangle
function getHistory_1(tool, x1, y1, x2, y2, color, thickness)
{
    var history = tool + " ";

    switch (tool) {
        case "pen_move":
        case "eraser_move":
        case "rectangle":
        case "pen_start":
        case "pen_end":
        case "eraser_start":
        case "eraser_end":
            history += x1 + " "
                + y1 + " "
                + x2 + " "
                + y2 + " "
                + color + " "
                + thickness;
            break;

        case "reset":
        case "ellipse":
            break;
    }

    return history;
}

// ellipse
function getHistory_2(tool, x, y, r1, r2, color, thickness)
{
    var history = tool + " ";

    switch (tool) {
        case "ellipse":
            history += x + " "
                + y + " "
                + r1 + " "
                + r2 + " "
                + color + " "
                + thickness;
            break;

        case "reset":
        case "pen_start":
        case "pen_end":
        case "eraser_start":
        case "eraser_end":
        case "pen_move":
        case "eraser_move":
        case "rectangle":
            break;
    }

    return history;
}

function resizeCanvasAll(isInit)
{
    const newWidth = Math.max(1, window.innerWidth || document.documentElement.clientWidth || 1);
    const newHeight = Math.max(1, window.innerHeight || document.documentElement.clientHeight || 1);

    canvas.width = newWidth;
    canvas.height = newHeight;

    bufCanvas.width = newWidth;
    bufCanvas.height = newHeight;

    remainCanvas.width = newWidth;
    remainCanvas.height = newHeight;

    ctx = canvas.getContext("2d");
    bufCtx = bufCanvas.getContext("2d");
    remainCtx = remainCanvas.getContext("2d");

    if (isInit) {
        remainCtx.clearRect(0, 0, canvas.width, canvas.height);
        return;
    }

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    bufCtx.clearRect(0, 0, bufCanvas.width, bufCanvas.height);
    remainCtx.clearRect(0, 0, remainCanvas.width, remainCanvas.height);

    if (typeof drawCanvasHistory === "function") {
        drawCanvasHistory(canvas, ctx, bufCanvas, bufCtx, remainCanvas, remainCtx, canvasHistory.list);
    }
}

function handleResize()
{
    resizeCanvasAll(false);
}

window.onload = function()
{
    canvas = document.getElementById("afcanvas");
    ctx = canvas.getContext("2d");

    bufCanvas = document.createElement("canvas");
    bufCtx = bufCanvas.getContext("2d");

    remainCanvas = document.createElement("canvas");
    remainCtx = remainCanvas.getContext("2d");

    resizeCanvasAll(true);

    canvas.addEventListener("mousedown", mouseListener);
    canvas.addEventListener("mousemove", mouseListener);
    canvas.addEventListener("mouseout", mouseListener);
    canvas.addEventListener("mouseup", mouseListener);

    window.addEventListener("resize", handleResize);

    initCanvas();
};

function makeRGB(r, g, b)
{
    r = Math.floor(r);
    g = Math.floor(g);
    b = Math.floor(b);
    return ["rgb(", r, ",", g, ",", b, ")"].join("");
}

function makeRGBA(r, g, b, a)
{
    r = Math.floor(r);
    g = Math.floor(g);
    b = Math.floor(b);
    return ["rgba(", r, ",", g, ",", b, ",", a, ")"].join("");
}

function getMousePos(event)
{
    const rect = canvas.getBoundingClientRect();

    return {
        x: Math.round((event.clientX - rect.left) * (canvas.width / rect.width)),
        y: Math.round((event.clientY - rect.top) * (canvas.height / rect.height))
    };
}

// For Exe => Html
function selectTool(tool)
{
    painter.updateTool(tool);
}

function selectLineColor(r, g, b)
{
    painter.updateColor(makeRGB(r, g, b));
}

function useBgAlpha(alpha)
{
    painter.updateAlpha(alpha);
    canvas.style.backgroundColor = makeRGBA(painter.bgR, painter.bgG, painter.bgB, painter.alpha);
}

function selectBgColor(r, g, b)
{
    painter.updateRGB(r, g, b);
    canvas.style.backgroundColor = makeRGBA(r, g, b, painter.alpha);
}

function selectThickness(thickness)
{
    painter.updateThickness(thickness);
}

function updatePainterInfo(tool, r, g, b, thickness)
{
    painter.updateTool(tool);
    painter.updateColor(makeRGB(r, g, b));
    painter.updateThickness(thickness);
}

function initCanvas()
{
    remainCtx.clearRect(0, 0, canvas.width, canvas.height);
    bufCtx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.clearRect(0, 0, canvas.width, canvas.height);
}

function resetCanvas()
{
    initCanvas();
    canvasHistory.clearHistory();
    canvasRedoHistory.clearHistory();
}

function mouseListener(event)
{
    if (event.button == 2) {
        return;
    }

    switch (event.type) {
        case "mousedown":
            if (painter.isDown) {
                break;
            }

            if (painter.isPrevUndo) {
                canvasRedoHistory.clearHistory();
                painter.isPrevUndo = false;

                var aSendData = {};
                aSendData["action_type"] = "enable_redo";
                SendData(JSON.stringify(aSendData));
            }

            painter.mouseDown(event);
            break;

        case "mouseup":
        case "mouseout":
            if (!painter.isDown) {
                break;
            }

            painter.mouseUp(event);
            break;

        case "mousemove":
            if (!painter.isDown) {
                break;
            }

            painter.mouseMove(event);
            break;
    }
}

function undo()
{
    if (canvasHistory.list.length < 1) {
        return;
    }

    painter.isPrevUndo = true;

    var lastHistory = canvasHistory.removeHistroy(true);
    canvasRedoHistory.addHistory(lastHistory, true);

    var lastHistoryArray = lastHistory.split(" ");

    if (lastHistoryArray[0].trim() == "pen_end") {
        while (canvasHistory.list.length > 0) {
            lastHistory = canvasHistory.removeHistroy(false);

            lastHistoryArray = lastHistory.split(" ");

            lastHistory = lastHistory.trim();
            if (lastHistory.length == 0) {
                continue;
            }

            canvasRedoHistory.addHistory(lastHistory);
            if (lastHistoryArray[0].trim() == "pen_start") {
                break;
            }
        }
    }

    if (lastHistoryArray[0].trim() == "eraser_end") {
        while (canvasHistory.list.length > 0) {
            lastHistory = canvasHistory.removeHistroy(false);

            lastHistoryArray = lastHistory.split(" ");

            lastHistory = lastHistory.trim();
            if (lastHistory.length == 0) {
                continue;
            }

            canvasRedoHistory.addHistory(lastHistory);
            if (lastHistoryArray[0].trim() == "eraser_start") {
                break;
            }
        }
    }

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    bufCtx.clearRect(0, 0, canvas.width, canvas.height);

    if (typeof drawCanvasHistory === "function") {
        drawCanvasHistory(canvas, ctx, bufCanvas, bufCtx, remainCanvas, remainCtx, canvasHistory.list);
    }
}

function redo()
{
    if (canvasRedoHistory.list.length < 1) {
        return;
    }

    var lastHistory = canvasRedoHistory.removeHistroy(true);
    canvasHistory.addHistory(lastHistory, true, true);

    var lastHistoryArray = lastHistory.split(" ");

    if (lastHistoryArray[0].trim() == "pen_start") {
        while (canvasRedoHistory.list.length > 0) {
            lastHistory = canvasRedoHistory.removeHistroy(false);

            if (lastHistory.length == 0) {
                continue;
            }

            canvasHistory.addHistory(lastHistory);

            lastHistoryArray = lastHistory.split(" ");

            if (lastHistoryArray[0].trim() == "pen_end") {
                break;
            }
        }
    }

    if (lastHistoryArray[0].trim() == "eraser_start") {
        while (canvasRedoHistory.list.length > 0) {
            lastHistory = canvasRedoHistory.removeHistroy(false);

            if (lastHistory.length == 0) {
                continue;
            }

            canvasHistory.addHistory(lastHistory);

            lastHistoryArray = lastHistory.split(" ");

            if (lastHistoryArray[0].trim() == "eraser_end") {
                break;
            }
        }
    }

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    bufCtx.clearRect(0, 0, canvas.width, canvas.height);

    if (typeof drawCanvasHistory === "function") {
        drawCanvasHistory(canvas, ctx, bufCanvas, bufCtx, remainCanvas, remainCtx, canvasHistory.list);
    }
}

// tool function

// pen
function penDown(event)
{
    if (painter.isDown) {
        return;
    }

    painter.isDown = true;

    var startPos = getMousePos(event);

    ctx.beginPath();
    ctx.globalCompositeOperation = "source-over";
    ctx.strokeStyle = painter.color;
    ctx.lineWidth = painter.thickness;
    ctx.lineJoin = "round";
    ctx.lineCap = "round";

    ctx.moveTo(startPos.x, startPos.y);
    ctx.stroke();

    var history = getHistory_1("pen_start", startPos.x, startPos.y, 0, 0, painter.color, painter.thickness);
    canvasHistory.addHistory(history);

    painter.x = startPos.x;
    painter.y = startPos.y;
}

function penMove(event)
{
    var currentPos = getMousePos(event);

    hueColorValue++;
    if (hueColorValue >= 360) {
        hueColorValue = 0;
    }

    ctx.beginPath();
    ctx.strokeStyle = painter.color;
    ctx.lineWidth = painter.thickness;
    ctx.moveTo(painter.x, painter.y);
    ctx.lineTo(currentPos.x, currentPos.y);
    ctx.stroke();
    ctx.closePath();

    var history = getHistory_1("pen_move", painter.x, painter.y, currentPos.x, currentPos.y, ctx.strokeStyle, painter.thickness);
    canvasHistory.addHistory(history);

    painter.x = currentPos.x;
    painter.y = currentPos.y;
}

function penUp(event)
{
    if (!painter.isDown) {
        return;
    }

    ctx.closePath();

    var history = getHistory_1("pen_end", 0, 0, 0, 0, painter.color, painter.thickness);
    canvasHistory.addHistory(history, true);

    painter.isDown = false;
}

// eraser
function eraserDown(event)
{
    if (painter.isDown) {
        return;
    }

    painter.isDown = true;

    ctx.beginPath();
    ctx.globalCompositeOperation = "destination-out";
    ctx.strokeStyle = painter.color;
    ctx.lineWidth = painter.thickness;
    ctx.lineJoin = "round";
    ctx.lineCap = "round";

    var startPos = getMousePos(event);

    painter.x = startPos.x;
    painter.y = startPos.y;

    ctx.moveTo(startPos.x, startPos.y);
    ctx.stroke();

    var history = getHistory_1("eraser_start", startPos.x, startPos.y, 0, 0, painter.color, painter.thickness);
    canvasHistory.addHistory(history);
}

function eraserMove(event)
{
    var currentPos = getMousePos(event);

    ctx.lineTo(currentPos.x, currentPos.y);
    ctx.stroke();

    var history = getHistory_1("eraser_move", painter.x, painter.y, currentPos.x, currentPos.y, painter.color, painter.thickness);
    canvasHistory.addHistory(history);

    painter.x = currentPos.x;
    painter.y = currentPos.y;
}

function eraserUp(event)
{
    if (!painter.isDown) {
        return;
    }

    painter.isDown = false;

    ctx.closePath();

    var history = getHistory_1("eraser_end", 0, 0, 0, 0, painter.color, painter.thickness);
    canvasHistory.addHistory(history, true);
}

// rectangle
function rectangleDown(event)
{
    if (painter.isDown) {
        return;
    }

    painter.isDown = true;

    var startPos = getMousePos(event);

    ctx.lineWidth = painter.thickness;
    ctx.globalCompositeOperation = "source-over";

    bufCtx.clearRect(0, 0, canvas.width, canvas.height);
    bufCtx.strokeStyle = painter.color;
    bufCtx.globalCompositeOperation = "source-over";
    bufCtx.lineWidth = painter.thickness;
    bufCtx.drawImage(canvas, 0, 0);

    painter.x = startPos.x;
    painter.y = startPos.y;
}

function rectangleMove(event)
{
    var currentPos = getMousePos(event);

    ctx.beginPath();
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(bufCanvas, 0, 0);

    ctx.strokeStyle = painter.color;
    ctx.lineWidth = painter.thickness;

    var box = {
        w: currentPos.x - painter.x,
        h: currentPos.y - painter.y
    };

    ctx.strokeRect(painter.x, painter.y, box.w, box.h);
    ctx.closePath();
}

function rectangleUp(event)
{
    if (!painter.isDown) {
        return;
    }

    painter.isDown = false;

    var currentPos = getMousePos(event);

    bufCtx.beginPath();
    bufCtx.strokeStyle = painter.color;
    bufCtx.lineWidth = painter.thickness;

    var box = {
        w: currentPos.x - painter.x,
        h: currentPos.y - painter.y
    };

    bufCtx.strokeRect(painter.x, painter.y, box.w, box.h);
    bufCtx.closePath();

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(bufCanvas, 0, 0);

    bufCtx.clearRect(0, 0, canvas.width, canvas.height);

    var history = getHistory_1("rectangle", painter.x, painter.y, currentPos.x, currentPos.y, painter.color, painter.thickness);
    canvasHistory.addHistory(history, true);
}

// ellipse
function ellipseDown(event)
{
    if (painter.isDown) {
        return;
    }

    painter.isDown = true;

    var startPos = getMousePos(event);

    ctx.lineWidth = painter.thickness;
    ctx.globalCompositeOperation = "source-over";

    bufCtx.clearRect(0, 0, canvas.width, canvas.height);
    bufCtx.strokeStyle = painter.color;
    bufCtx.globalCompositeOperation = "source-over";
    bufCtx.lineWidth = painter.thickness;
    bufCtx.drawImage(canvas, 0, 0);

    painter.x = startPos.x;
    painter.y = startPos.y;
}

function ellipseMove(event)
{
    var currentPos = getMousePos(event);

    ctx.beginPath();
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(bufCanvas, 0, 0);

    ctx.strokeStyle = painter.color;
    ctx.lineWidth = painter.thickness;

    var ellipse = {
        x: Math.round((painter.x + currentPos.x) / 2),
        y: Math.round((painter.y + currentPos.y) / 2),
        r1: Math.round(Math.abs(currentPos.x - painter.x) / 2),
        r2: Math.round(Math.abs(currentPos.y - painter.y) / 2)
    };

    ctx.ellipse(ellipse.x, ellipse.y, ellipse.r1, ellipse.r2, 0, 0, 2 * Math.PI);
    ctx.stroke();
    ctx.closePath();
}

function ellipseUp(event)
{
    if (!painter.isDown) {
        return;
    }

    painter.isDown = false;

    var currentPos = getMousePos(event);

    bufCtx.beginPath();
    bufCtx.strokeStyle = painter.color;
    bufCtx.lineWidth = painter.thickness;

    var ellipse = {
        x: Math.round((painter.x + currentPos.x) / 2),
        y: Math.round((painter.y + currentPos.y) / 2),
        r1: Math.round(Math.abs(currentPos.x - painter.x) / 2),
        r2: Math.round(Math.abs(currentPos.y - painter.y) / 2)
    };

    bufCtx.ellipse(ellipse.x, ellipse.y, ellipse.r1, ellipse.r2, 0, 0, 2 * Math.PI);
    bufCtx.stroke();
    bufCtx.closePath();

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(bufCanvas, 0, 0);

    bufCtx.clearRect(0, 0, canvas.width, canvas.height);

    var history = getHistory_2("ellipse", ellipse.x, ellipse.y, ellipse.r1, ellipse.r2, painter.color, painter.thickness);
    canvasHistory.addHistory(history, true);
}