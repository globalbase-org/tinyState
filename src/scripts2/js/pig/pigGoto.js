//:IMPORT pig/pigPrimitive.js

function pigGoto(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigGoto");
}


piggybackTurtle.top.bind(
    null,
    "goto",
    new pigData("pn:primitive",pigGoto));

stdObject.defClass
(
    "pigGoto",
    pigGoto,
    pigPrimitive,
    {


	/*=====================================================
	 *
	 *    pigGoto STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    this.enableCatchAbort();
	    this.callFunc = new pigActivate(this,["all"]);
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.getType() == "error" ) {
		ev.source = this;
		this.parent.eventHandler(ev);
		return "doFIN_START";
	    }
	    this.args[1] = pigActivate.normalize(this.args[1]);
	    if ( typeof(this.args[1]) != "string" ){
		this.i_sendResult("type missmatch arg 1 (string is required)","error");
		return "doFIN_START";
	    }
	    if ( this.args.length >= 3 ) {
		this.i_sendResult(
		    new pigData("error",
				"control:goto:"+this.args[1],
				this.args[2]));
	    }
	    else {
		this.i_sendResult(
		    new pigData("error","control:goto:"+this.args[1]));
	    }
	    return "doFIN_START";
	},

    }
);


