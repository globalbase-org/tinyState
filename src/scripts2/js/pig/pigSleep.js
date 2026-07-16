//:IMPORT pig/pigPrimitive.js

function pigSleep(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigSleep");
}

piggybackTurtle.top.bind(
    null,
    "sleep",
    new pigData("pn:primitive",pigSleep));


stdObject.defClass
(
    "pigSleep",
    pigSleep,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigSleep STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.i_abort = false;
	    this.callFunc = new pigEval(this,this.env,this.param[1]);
	    return "INI_RET";
	},
	INI_RET : function(ev) {
	    if ( ev.type == "abort" ) {
		this.i_abort = true;
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    if ( this.i_abort ) {
		this.i_sendResult("aborted","error");
		return "doFIN_START";
	    }
	    if ( ev.msg.getType() != "number" ) {
		var msg = "type missmatch of arg 1 SLEEP : ["+ev.msg.getType()+"]";
		this.dump(msg);
		this.i_sendResult(msg,"error");
		return "doFIN_START";
	    }
	    
	    stdInterval.wait(this,ev.msg.d,"return");
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( ev.type == "abort" ) {
		this.i_sendResult("aborted","error");
		return "doFIN_START";
	    }
	    if ( ev.type != "return" )
		return;
	    this.i_sendResult("OK");
	    return "doFIN_START";
	},
    }
);


