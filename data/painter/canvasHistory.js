
var historyTable =
{
    reset: runReset,
    pen_start: runPenStart,
    pen_end: runPenEnd,
    pen_move: runPenLine,
    eraser_start: runEraserStart,
    eraser_end: runEraserEnd,
    eraser_move: runEraserLine,
    rectangle: runRectangle,
    ellipse: runEllipse
}

function runHistory(history, bufCtx) {

    var type = history[0]
    historyTable[type](history, bufCtx);
}


function initCanvasHistory(ctx) {
    ctx.beginPath();
    ctx.globalCompositeOperation = "source-over";
    ctx.closePath();
};

function drawCanvasHistory(canvas, ctx, bufCanvas, bufCtx, remainCanvas,remainCtx,list) {
    
    initCanvasHistory(ctx);
    initCanvasHistory(bufCtx);

    bufCtx.drawImage(remainCanvas,0,0);

    list.forEach(function (event) {
        var cnt = event.length;
        if (cnt > 0) {
            var history = event.split(' ');
            runHistory(history, bufCtx);
        }
    });

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(bufCanvas, 0, 0);
};


function drawCanvasHistory2(canvas, ctx, list) {
    
    initCanvasHistory(ctx);

    list.forEach(function (event) {
        var cnt = event.length;
        if (cnt > 0) {
            var history = event.split(' ');
            runHistory(history, ctx);
        }
    });
};

// reset History
function runReset(history, bufCtx) { }

// pen History
function runPenStart(history, bufCtx) 
{
    bufCtx.beginPath();
	bufCtx.lineJoin = 'round';
	bufCtx.lineCap  = 'round';
    bufCtx.globalCompositeOperation = "source-over";
    bufCtx.strokeStyle = history[5];
    bufCtx.lineWidth = history[6];
	
    bufCtx.moveTo(history[1], history[2]);
	bufCtx.stroke();
	
}
function runPenEnd(history, bufCtx) 
{
	bufCtx.closePath();	
}
function runPenLine(history, bufCtx) 
{

	bufCtx.beginPath();
    bufCtx.strokeStyle = history[5];
    bufCtx.moveTo(history[1], history[2]);
    bufCtx.lineTo(history[3], history[4]);
    bufCtx.stroke();
	bufCtx.closePath();	

}

// eraser Histroy
function runEraserStart(history, bufCtx) 
{
	bufCtx.beginPath();
    bufCtx.globalCompositeOperation = "destination-out";
	bufCtx.strokeStyle = history[5];
    bufCtx.lineWidth = 15;
    bufCtx.lineHeight= 15;

    bufCtx.moveTo(history[1], history[2]);
    bufCtx.stroke();
	
}

function runEraserEnd(history, bufCtx) 
{	
    bufCtx.closePath();
}

function runEraserLine(history, bufCtx) {
	
    //bufCtx.lineTo(history[1], history[2]);
    bufCtx.lineTo(history[3], history[4]);
    bufCtx.stroke();
	
}

// rectangle Histroy
function runRectangle(history, bufCtx) {

    bufCtx.beginPath();
    bufCtx.globalCompositeOperation = "source-over";
    bufCtx.strokeStyle = history[5];
    bufCtx.lineWidth = history[6];

    var rcWidth = history[3] - history[1];
    var rcHeight = history[4] - history[2];

    bufCtx.strokeRect(history[1], history[2], rcWidth, rcHeight);
    bufCtx.closePath();
}

// rectangle Histroy
function runEllipse(history, bufCtx) {

    bufCtx.beginPath();
    bufCtx.globalCompositeOperation = "source-over";
    bufCtx.strokeStyle = history[5];
    bufCtx.lineWidth = history[6];

    var ellipse = {
        x: parseInt(history[1]),
        y: parseInt(history[2]),
        a: parseInt(history[3]),
        b: parseInt(history[4])
    };

    bufCtx.ellipse(ellipse.x, ellipse.y, ellipse.a, ellipse.b, 0, 0, 2 * Math.PI);
    bufCtx.stroke();
    bufCtx.closePath();
}