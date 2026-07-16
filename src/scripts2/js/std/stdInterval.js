
//:IMPORT std/stdObject.js
//:EXPORT stdInterval


function stdInterval()
{
    stdObject.apply(this);
    var d = new Date();
    stdInterval.i_startTime = d.getTime();
}

stdInterval.setTimeout_count = 0;

stdInterval.nowMsec = function() {
    var d = new Date();
    return d.getTime() - stdInterval.i_startTime;
};

stdInterval.now = function() {
    return stdInterval.nowMsec() * 1000;
};

stdInterval.stop = false;

stdInterval.i_setTimeout = function() {
    if ( stdInterval.i_justTimeQueue.length > 0 ) {
	stdInterval.i_queue.unshift(stdInterval.i_justTimeQueue);
	stdInterval.i_justTimeQueue = [];
    }
    if ( stdInterval.i_queue.length == 0 )
	return;
    var node = stdInterval.i_queue[0];
    if ( node.setting )
	return;
    var n;
    for ( n = node ; n ; n = n.next )
	n.setting = true;
    var expire = node.expire;
    var rinterval = expire - stdInterval.nowMsec();
    if ( rinterval < 0 )
	rinterval = 0;
    stdInterval.setTimeout_count ++;
    if ( stdInterval.stop )
	return;
    //console.log("stdInterval setTimeout -- "+rinterval+" "+node.caller.className);
    setTimeout('stdInterval.i_invoke('+expire+')',rinterval);
}


stdInterval.wait = function(caller,interval,eventType) {
    if ( interval < 0 )
	return -1;
    var nowtim = stdInterval.nowMsec();
    var expire = nowtim + Math.ceil(interval/1000);
    var node = {
	expire : expire,
	caller : caller,
	eventType : eventType,
	setting : false
    };
    if ( expire <= nowtim ) {
	node.next = stdInterval.i_justTimeQueue;
	stdInterval.i_justTimeQueue = node;
	return;
    }
    var que = stdInterval.i_queue;
    var len = que.length;
    for ( var i = 0 ; i < len; i ++ )
	if ( que[i].expire >= expire )
	    break;
    if ( i == len ) {
	stdInterval.i_queue.push(node);
    }
    else if ( que[i].expire == expire ) {
	node.next = que[i];
	node.setting = que[i].setting;
	que[i] = node;
    }
    else {
	stdInterval.i_queue
	    = que.slice(0,i).concat([node]).concat(que.slice(i));
    }
    //    console.log("stdIterval.wait -- "+caller.className);
    stdInterval.i_setTimeout();
};

stdInterval.i_invoke = function(expire) {
    var nowtim = stdInterval.nowMsec();
    //console.log("INVOKE!!");
    if ( expire > nowtim ) {
	var rinterval = expire-nowtim;
	setTimeout('stdInterval.i_invoke('+expire+')',rinterval);
	return;
    }
    for ( ; stdInterval.i_queue.length > 0 ; ) {
	var node = stdInterval.i_queue[0];
	if ( node.expire > nowtim  ) {
	    //console.log("BREAK "+node.caller.className);
	    break;
	}
	for ( ; node ; ) {
	    //console.log("INVOKE---"+node.caller.className);
	    node.caller.eventHandler(
		new stdEvent(node.eventType,node.caller,nowtim*1000));
	    node = node.next;
	}
	stdInterval.i_queue.shift();
    }
    //    console.log("INVOKE-setTimeout");
    stdInterval.i_setTimeout();
}

stdInterval.i_queue = [];
stdInterval.i_justTimeQueue = [];
stdInterval.i_startTime;

stdObject.defClass
(
    "stdInterval",
    stdInterval,
    stdObject
);


