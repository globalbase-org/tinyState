
//:IMPORT std/stdObject.js

function stdSemaphore(count)
{
    if ( arguments.length == 0 )
        return;
    stdObject.apply(this);
    this.count = count;
    this.i_waitQueue = [];
    this.i_waitHash = {};
}

stdObject.defClass
(
    "stdSemaphore",
    stdSemaphore,
    stdObject,
    {
	lastCaller : null,

	get : function(caller,nextState) {
	    if ( this.count == 0 ) {
		if ( this.i_waitHash[caller.objId()] )
		    return;
		this.i_waitQueue.push(caller);
		this.i_waitHash[caller.objId()] = true;
		return;
	    }
	    this.count --;
	    if ( this.count == 0 )
		this.lastCaller = caller;
	    return nextState;
	},
	release : function() {
	    this.count ++;
	    this.lastCaller = null;
	    var target;
	    for ( ; this.i_waitQueue.length ; ) {
		target = this.i_waitQueue.shift();
		delete this.i_waitHash[target.objId()];
		if ( target.state(["ZOM"]) )
		    continue;
		target.wakeup();
		return;
	    }
	}
    }
);



