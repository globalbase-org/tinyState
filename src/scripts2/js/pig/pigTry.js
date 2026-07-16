//:IMPORT pig/pigPrimitive.js

function pigTry(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigTry");
}

piggybackTurtle.top.bind(
    null,
    "try",
    new pigData("pn:primitive",pigTry));


stdObject.defClass
(
    "pigTry",
    pigTry,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigTry STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    return "doACT_START";
	},
	ACT_START : function(ev) {
	    this.callFunc = new pigEval(this,this.env,this.param[1]);
	    return "ACT_START_RET";
	},
	ACT_START_RET : function(ev) {
	    if ( ev.type == "abort" ) {
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    var type = ev.msg.getType();
	    if ( type != "error" ) {
		ev.source = this;
		this.parent.eventHandler(ev);
		return "doFIN_START";
	    }
	    if ( ev.msg.d.indexOf("control:") == 0 ) {
		ev.source = this;
		this.parent.eventHandler(ev);
		return "doFIN_START";
	    }
	    this.nenv = new piggybackTurtle(this.env,this);
	    this.nenv.bind(this,"v.error",ev.msg.d);
	    this.callFunc = new pigEval(this,this.nenv,this.param[2]);
	    return "ACT_START_RET_2";
	},
	ACT_START_RET_2 : function(ev) {
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


