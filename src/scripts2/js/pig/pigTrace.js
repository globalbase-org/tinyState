//:IMPORT pig/pigPrimitive.js

function pigTrace(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigTrace");
}

piggybackTurtle.top.bind(
    null,
    "trace",
    new pigData("pn:primitive",pigTrace));


stdObject.defClass
(
    "pigTrace",
    pigTrace,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigTrace STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.args[1] = pigActivate.normalize(this.param[1]);
	    this.callFunc = new pigEval(this,this.env,this.args[1]);
	    return "INI_TAG";
	},
	INI_TAG : function(ev) {
	    if ( ev.type == "abort" ) {
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    switch ( ev.msg.getType() ) {
	    case "error":
		ev.source = this;
		this.parent.eventHandler(ev);
		return "doFIN_START";
	    case "string":
		break;
	    default:
		var msg = "type missmatch for TRACE tag";
		this.dump(msg);
		this.i_sendResult(msg,"error");
		return "doFIN_START";
	    }
	    this.args[2] = pigActivate.normalize(this.param[2]);
	    this.callFunc = new pigEval(this,this.env,this.args[2]);
	    this.callFunc.traceOn(ev.msg.d);
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( ev.type == "abort" ) {
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    ev.source = this;
	    this.parent.eventHandler(ev);
	    return "doFIN_START";
	},

    }
);


