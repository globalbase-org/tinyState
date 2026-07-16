//:IMPORT pig/pigPrimitive.js

function pigSq(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigSq");
}


piggybackTurtle.top.bind(
    null,
    "sq",
    new pigData("pn:primitive",pigSq));

stdObject.defClass
(
    "pigSq",
    pigSq,
    pigPrimitive,
    {


	/*=====================================================
	 *
	 *    pigSq STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    this.ptr = 1;
	    return "doACT_START";
	},

	ACT_START : function(ev) {
	    this.callFunc = new pigEval(
		this,
		this.env,
		this.param[this.ptr]);
	    return "ACT_START_RET";
	},
	ACT_START_RET : function(ev) {
	    if ( ev.type == "abort" ) {
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    this.ret = ev;
	    if ( ev.msg.getType() == "error" )
		return "doFIN_START";
	    this.ptr ++;
	    if ( this.ptr >= this.param.length )
		return "doFIN_START";
	    return "doACT_START";
	},
	FIN_START : function(ev) {
	    this.ret.source = this;
	    this.parent.eventHandler(this.ret);
	    this.ret = 0;
	    return "doFIN_pigPrimitive_START";
	},
    }
);


