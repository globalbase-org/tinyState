//:IMPORT pig/piggybackTurtle.js
//:IMPORT ts/tinyState.js
//:IMPORT ts/tsWatchDog.js

function pigPrimitive(parent,env,param) {
    if (arguments.length == 0)
	return;
    this.env = env;
    this.param = param;
    this.args = [];
    if ( parent.sync )
	this.sync = parent.sync;
    else
	this.sync = null;
    this.callFunc = null;
    for ( ix in this.param ) {
	if ( typeof(this.param[ix]) != "string" )
	    continue;
	if ( this.param[ix].indexOf("trace=") == 0 ) {
	    this.trace(this.param[ix].substr(6));
	    break;
	}
    }
    tinyState.apply(this, [parent]);
    this.setEventFilter(this.i_enableWatchDog);
    this.initial("pigPrimitive");
}


stdObject.defClass
(
    "pigPrimitive",
    pigPrimitive,
    tinyState,
    {
	i_sendResult : function(data,type) {
	    if ( type == null )
		this.parent.eventHandler(
		    new stdEvent(
			"return",
			this,
			pigData.make(data)));
	    else
		this.parent.eventHandler(
		    new stdEvent(
			"return",
			this,
			new pigData(type,data)));
	},

	i_setResultEvent : function(data,type) {
	    if ( type == null )
		this.i_sendEvent = new stdEvent(
		    "return",
		    this,
		    pigData.make(data));
	    else
		this.i_sendEvent = new stdEvent(
		    "return",
		    this,
		    new pigData(type,data));
	},

	i_sendResultEvent : function() {
	    this.parent.eventHandler(this.i_sendEvent);
	    this.i_sendEvent = null;
	},

	abort : function(caller) {
	    this.eventHandler(
		new stdEvent("abort",caller));
	},

	i_abort : false,

	i_enableWatchDog : function(ev) {
	    if ( ev.type == "abort" && !this.watchDog )
		new tsWatchDog(this);
	    return ev;
	},

	i_catchAbort : function(ev) {
	    if ( ev.type != "abort" )
		return ev;
	    this.i_abort = true;
	    if ( !this.callFunc )
		return ev;
	    if ( Array.isArray(this.callFunc) ) {
		var i;
		for ( i in this.callFunc )
		    this.callFunc[i].eventHandler(ev);
	    }
	    else
		this.callFunc.eventHandler(ev);
	    return ev;
	},

	enableCatchAbort : function() {
	    this.setEventFilter(this.i_catchAbort);
	},

	/*=====================================================
	 *
	 *    pigPrimitive STATE MACHINE
	 *
	 *=====================================================*/


	FIN_START : function(ev) {
	    return "doFIN_pigPrimitive_START";
	},
	FIN_pigPrimitive_START : function(ev) {
	    if ( typeof(this.nenv) == "object" ) {
		if ( Array.isArray(this.nenv) )
		    for ( ix in this.nenv )
			this.nenv[ix].destroy();
		else
		    this.nenv.destroy();
	    }
	    this.nenv = null;
	    this.env = null;
	    this.param = null;
	    this.args = null;
	    this.callFunc = null;
	    this.sync = null;
	    return "doFIN_TINYSTATE_START";
	},
    }
);


