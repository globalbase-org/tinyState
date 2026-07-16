//:IMPORT pig/pigPrimitive.js

function pigTraceError(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigTraceError");
}

piggybackTurtle.top.bind(
    null,
    "traceError",
    new pigData("pn:primitive",pigTraceError));


stdObject.defClass
(
    "pigTraceError",
    pigTraceError,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigTraceError STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.enableCatchAbort();
	    this.callFunc = new pigEval(this,this.env,this.param[1]);
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.getType() == "error" )
		ev.msg.trace = true;
	    ev.source = this;
	    this.parent.eventHandler(ev);
	    return "doFIN_START";
	},
    }
);


