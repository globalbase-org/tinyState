//:IMPORT pig/pigPrimitive.js

function pigFor(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigFor");
}


piggybackTurtle.top.bind(
    null,
    "for",
    new pigData("pn:primitive",pigFor));

stdObject.defClass
(
    "pigFor",
    pigFor,
    pigPrimitive,
    {


	/*=====================================================
	 *
	 *    pigFor STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    this.i_abort = false;
	    this.ret = new stdEvent(
		"return",
		this,
		new pigData("pure",null));
	    return "doACT_START";
	},

	ACT_START : function(ev) {
	    this.callFunc = new pigEval(
		this,
		this.env,
		this.param[1]);
	    return "ACT_START_RET";
	},
	ACT_START_RET : function(ev) {
	    if ( ev.type == "abort" ) {
		this.i_abort = true;
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.getType() == "error" ) {
		this.ret = ev;
		return "doFIN_START";
	    }
	    if ( this.i_abort )
		return "doFIN_ABORT";
	    this.ptr = 2;
	    if ( ev.msg.getType().indexOf("pn:") == 0 )
		return "doACT_LOOP";
	    if ( ev.msg.d )
		return "doACT_LOOP";
	    return "doFIN_START";
	},
	ACT_LOOP : function(ev) {
	    if ( this.ptr >= this.param.length )
		return "doACT_START";
	    this.callFunc = new pigEval(
		this,
		this.env,
		this.param[this.ptr]);
	    return "ACT_LOOP_RET";
	},
	ACT_LOOP_RET : function(ev) {
	    if ( ev.type == "abort" ) {
		this.i_abort = true;
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.getType() == "error" ) {
		if ( ev.msg.d == "control:continue" )
		    return "doACT_START";
		if ( ev.msg.d == "control:break" )
		    return "doFIN_START";
		this.ret = ev;
		return "doFIN_START";
	    }
	    if ( this.i_abort )
		return "doFIN_ABORT";
	    this.ret = ev;
	    this.ptr ++;
	    return "doACT_LOOP";
	},
	FIN_ABORT : function(ev) {
	    this.i_sendResult("aborted","error");
	    return "doFIN_pigPrimitive_START";
	},
	FIN_START : function(ev) {
	    this.ret.source = this;
	    this.parent.eventHandler(this.ret);
	    this.ret = 0;
	    return "doFIN_pigPrimitive_START";
	},
    }
);


