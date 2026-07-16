//:IMPORT ts/tinyState.js
//:EXPORT ts/tsWatchDog.js


function tsWatchDog(parent)
{
    if ( arguments.length == 0 )
        return;
    tinyState.apply(this,[parent]);
    this.initial("tsWatchDog");
};


stdObject.defClass
(
    "tsWatchDog",
    tsWatchDog,
    tinyState,
    {
	i_expire : 0,
	i_finish : false,
	ind : 0,

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
//this.trace(this.parent.className);
	    if ( this.parent.watchDog )
		return "doFIN_START";
	    this.parent.watchDog = this;
	    this.parent.listen(this,"state");
	    return "doACT_PREV";
	},
	ACT_PREV : function(ev) {
	    var n = stdInterval.now();
	    var interval = 10*1000*1000;
	    this.i_expire = n + interval;
	    stdInterval.wait(this,interval,"timer");
	    return "doACT_START";
	},
	ACT_TINYSTATE_START : function(ev) {
	    if ( this.parent.state(["ZOM"]) )
		return "doFIN_PREV";
	    if ( this.i_expire <= stdInterval.now() ) {
		if ( this.ind > 0 ) {
		    this.ind --;
		    return "doACT_PREV";
		}
		var output = "";
		var p;
		this.i_finish = true;
		for ( p = this.parent ; p ; p = p.parent )  {
		    var ind;
		    ind = -1;
		    if ( p.watchDog && p.watchDog !== this ) {
			ind = p.watchDog.ind;
			p.watchDog.ind = 2;
		    }
		    if ( p.gid )
			output += "(" + p.objId() + "):" + p.gid + ":" + p.className + ":" + p.state() + " << ";
		    else
			output += "(" + p.objId() + "):" + p.className + ":" + p.state() + " << ";
		}
		if ( this.ind )
		    this.ind --;
		this.dump("WATCH DOG "+output);
		return "doACT_PREV";
	    }
	    return "ACT_START";
	},
	FIN_PREV : function(ev) {
	    if ( this.i_finish ) {
		var output = "";
		var p;
		for ( p = this.parent ; p ; p = p.parent )  {
		    if ( p.watchDog )
			p.watchDog.ind = 2;
		    if ( p.gid )
			output += p.gid + ":" + p.className + ":" + p.state() + " << ";
		    else
			output += p.className + ":" + p.state() + " << ";
		}
		this.dump("WATCH DOG FINISH "+output);
	    }

	    this.parent.watchDog = null;
	    return "doFIN_START";
	},
	FIN_START : function(ev) {
	    return "doFIN_TINYSTATE_START";
	},
    }
);
