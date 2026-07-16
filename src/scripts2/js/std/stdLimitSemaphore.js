
//:IMPORT std/stdObject.js
//:IMPORT std/stdInterval.js

function stdLimitSemaphore(limit,min,max)
{
//this.dump("LIMIT SEMAPHORE");
    if ( arguments.length == 0 )
        return;
    stdObject.apply(this);
    if ( !min )
	min = 1500000;
    if ( !max )
	max = 2000000;
    if ( !limit )
	limit = 1;
    this.min = min;
    this.max = max;
    this.v_value = (min+max)/2;
    this.last_update = stdInterval.now();
    this.limit = limit;
    this.count = 0;
//this.dump("COUNT "+this.count);
    this.last = 0;
    this.i_waitQueue = [];
}

stdObject.defClass
(
    "stdLimitSemaphore",
    stdLimitSemaphore,
    stdObject,
    {
	i_RATE : 0.0000001,
	i_UPDATE_INTERVAL : 1000000,
	i_sorted : false,

	i_sortFunc : function(a,b) {
	    var a_res = a.priority();
	    var b_res = b.priority();
	    if ( a_res < b_res )
		return -1;
	    if ( a_res > b_res )
		return 1;
	    return 0;
	},
	
	get : function(caller,nextState) {
	    if ( this.count >= this.limit ) {
		this.i_waitQueue.push(caller);
		this.i_sorted = false;
		return;
	    }
	    this.count ++;
//this.dump("GET SEM "+this.count+" limit="+this.limit);
	    return nextState;
	},
	release : function() {
	    this.count --;
//this.dump("RELEASE SEM "+this.count+" limit="+this.limit);
	    var target;
	    var sub = this.limit - this.count;
	    if ( sub <= 0 )
		return;
	    if ( sub > this.i_waitQueue.length )
		sub = this.i_waitQueue.length;
	    if ( !this.i_sorted ) {
		this.i_waitQueue.sort(this.i_sortFunc);
		this.i_sorted = true;
	    }
	    var i = 0;
	    for ( ; i < sub && this.i_waitQueue.length ; ) {
		target = this.i_waitQueue.shift();
		if ( target.state(["ZOM"]) )
		    continue;
		target.wakeup();
		i ++;
	    }
	},
	value : function(v_value) {
	    var n;
	    var ui;
	    var delta;
	    n = stdInterval.now();
	    delta = (n - this.last)*this.i_RATE;
	    if ( delta > 1 )
		delta = 1;
	    this.v_value
		= this.v_value*(1-delta)
		+ v_value*delta;
	    ui = (n-this.last_update)/this.i_UPDATE_INTERVAL;
	    ui = Math.ceil(ui);
	    if ( this.v_value < this.min )
		this.limit += ui;
	    else if ( this.v_value > this.max )
		this.limit -= ui;
	    if ( this.limit < 1 )
		this.limit = 1;
	    if ( this.limit > 20 )
		this.limit = 20;
	    this.last_update = n -
		((n-this.last_update) % this.i_UPDATE_INTERVAL);
	    this.last = n;
	},
    }
);



