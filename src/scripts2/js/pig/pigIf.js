//:IMPORT pig/pigPrimitive.js

function pigIf(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigIf");
}

piggybackTurtle.top.bind(
    null,
    "if",
    new pigData("pn:primitive",pigIf));


stdObject.defClass
(
    "pigIf",
    pigIf,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigIf STATE MACHINE
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
	    if ( type == "error" ) {
		ev.source = this;
		this.parent.eventHandler(ev);
		return "doFIN_START";
	    }
	    if ( type.indexOf("pn:") == 0 )
		this.callFunc = new pigEval(this,this.env,this.param[2]);
	    else if ( ev.msg.d )
		this.callFunc = new pigEval(this,this.env,this.param[2]);
	    else
		this.callFunc = new pigEval(this,this.env,this.param[3]);
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


