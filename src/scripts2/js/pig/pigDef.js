//:IMPORT pig/pigPrimitive.js

function pigDef(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigDef");
}

piggybackTurtle.top.bind(
    null,
    "def?",
    new pigData("pn:primitive",pigDef));


stdObject.defClass
(
    "pigDef",
    pigDef,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigDef STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.enableCatchAbort();
	    return "doACT_START";
	},
	ACT_START : function(ev) {
	    this.callFunc = new pigEval(this,this.env,this.param[1]);
	    return "ACT_START_RET";
	},
	ACT_START_RET : function(ev) {
	    if ( ev.type != "return" )
		return;
	    var type = ev.msg.getType();
	    if ( type == "error" ) {
		ev.source = this;
		this.i_sendEvent = ev;
		return "doFIN_START";
	    }
	    if ( ev.msg.getType() != "string" ) {
		this.i_setResultEvent("def : type missmatch","error");
		return "doFIN_START";
	    }
	    var symbol = ev.msg.d;
	    var ret = this.env.refer(this,symbol);
	    if ( ret.getType() != "error" ) {
		this.i_setResultEvent(true);
		return "doFIN_START";
	    }
	    this.i_setResultEvent(false);

	    if ( !this.sync )
		return "doFIN_START";

	    var pos = symbol.indexOf("..");
	    if ( pos >= 0 )
		symbol = symbol.substr(0,pos);
	    this.i_trigger = "d."+symbol.substr(2);
	    this.env.bind(this,this.i_trigger);
	    this.i_ret = this.env.refer(this,this.i_trigger);
	    return "doACT_AST_TYPE";
	},
	ACT_AST_TYPE : function(ev) {
	    return "doFIN_START";
	},
	FIN_START : function(ev) {
	    if ( this.i_abort )
		this.i_sendResult("aborted","error");
	    else 
		this.i_sendResultEvent();
	    this.i_ret = null;
	    return "doFIN_TINYSTATE_START";
	}
    }
);


