//:IMPORT pig/pigPrimitive.js
//:IMPORT pig/co_pigCatchAbort.js
/*
 ["cacheAbort",condition,func1]
 ["cacheAbort",condition,func1,func2]
 */

function pigCacheAbort(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigCacheAbort");
}


piggybackTurtle.top.bind(
    null,
    "catchAbort",
    new pigData("pn:primitive",pigCacheAbort));

stdObject.defClass
(
    "pigCacheAbort",
    pigCacheAbort,
    pigPrimitive,
    {


	/*=====================================================
	 *
	 *    pigCacheAbort STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.i_condition = true;
	    this.i_coRet = null;
	    this.i_abort = false;
	    this.callFunc = [];
	    this.callFunc[0] = new co_pigCatchAbort(
		this,
		this.env,
		["co",this.param[1]]);
	    this.callFunc[1] = new pigEval(
		this,
		this.env,
		this.param[2]);
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( ev.type == "abort" ) {
		this.i_abort = true;
		if ( !this.i_condition ) {
		    this.callFunc[1].eventHandler(ev);
		    return "doACT_SEND_ABORT";
		}
		return "ACT_RECV_ABORT";
	    }
	    if ( ev.type != "return" )
		return;
	    if ( ev.source === this.callFunc[0] ) {
		this.callFunc[0] = null;
		this.i_sendEvent = this.i_coRet;
		return "doFIN_START";
	    }
	    this.callFunc[1] = null;
	    ev.source = this;
	    this.i_sendEvent = ev;
	    if ( ev.msg.getType() == "error" )
		return "doFIN_START";
	    return "doACT_ABORT_DO";
	},
	ACT_RECV_ABORT : function(ev) {
	    if ( ev.type != "return" ) {
		if ( !this.i_condition ) {
		    this.callFunc[1].abort();
		    return "doACT_SEND_ABORT";
		}
		return;
	    }
	    if ( ev.source === this.callFunc[0] ) {
		this.callFunc[0] = null;
		this.i_sendEvent = this.i_coRet;
		return "doFIN_START";
	    }
	    this.callFunc[1] = null;
	    ev.source = this;
	    this.i_sendEvent = ev;
	    if ( ev.msg.getType() == "error" )
		return "doFIN_START";
	    return "doACT_ABORT_DO";
	},
	ACT_SEND_ABORT : function(ev) {
	    if ( ev.type != "return" )
		return;
	    this.i_sendEvent = ev;
	    return "doACT_ABORT_DO";
	},
	ACT_ABORT_DO : function(ev) {
	    if ( this.param.length < 4 )
		return "doFIN_START";
	    this.nenv = new piggybackTurtle(this.env,this);
	    this.nenv.bind(this,"v.return",this.i_sendEvent.msg);
	    this.nenv.bind(this,"v.abort",this.i_abort);
	    this.callFunc[1] = new pigEval(
		this,
		this.nenv,
		this.param[3]);
	    return "ACT_ABORT_RET";
	},
	ACT_ABORT_RET : function(ev) {
	    if ( ev.type != "return" )
		return;
	    ev.source = this;
	    this.i_sendEvent = ev;
	    this.callFunc[1] = null;
	    return "doFIN_START";
	},

	FIN_START : function(ev) {
	    for ( i = 0 ; i < this.callFunc.length ; i ++ ) {
		if ( !this.callFunc[i] )
		    continue;
		this.callFunc[i].listen(this,"state");
		this.callFunc[i].abort();
	    }
	    return "doFIN_START_WAIT";
	},
	FIN_START_WAIT : function(ev) {
	    for ( i = 0 ; i < this.callFunc.length ; i ++ ) {
		if ( !this.callFunc[i] )
		    continue;
		if ( !this.callFunc[i].state(["ZOM"]) )
		    return;
	    }
	    this.i_sendResultEvent();
	    return "doFIN_pigPrimitive_START";
	},
    }
);


