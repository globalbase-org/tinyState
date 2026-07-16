//:IMPORT pig/pigPrimitive.js

function pigRefer(parent,env,param) {
    if (arguments.length == 0)
	return;
    this.i_syncTerminal = {};
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigRefer");
}

piggybackTurtle.top.bind(
    null,
    "refer",
    new pigData("pn:primitive",pigRefer));


stdObject.defClass
(
    "pigRefer",
    pigRefer,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigRefer STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.callFunc = new pigActivate(this,["all"]);
	    return "doINI_WAIT_ACTIVATE";
	},
	INI_WAIT_ACTIVATE : function(ev) {
	    if ( ev.type == "abort" ) {
		this.i_abort = true;
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    return "doACT_START";
	},
	ACT_START : function(ev) {
	    if ( this.args[2].getType() == "error" ) {
		this.i_sendResult(this.args[2]);
		return "doFIN_START";
	    }
	    if ( this.args[2].getType() != "string" ) {
		var msg = "3rd arg type missmatch "+this.args[2].getType()+" in refer($) command";
		this.dump(msg);
		this.i_sendResult(msg,"error");
		return "doFIN_START";
	    }
	    if ( this.args[1].getType() == "error" ) {
		this.i_sendResult(this.args[2]);
		return "doFIN_START";
	    }
	    if ( this.args[1].getType() != "number" ) {
		var msg = "2nd arg type missmatch "+this.args[1].getType()+" in refer($) command";
		this.dump(msg);
		this.i_sendResult(msg,"error");
		return "doFIN_START";
	    }
	    var ref = this.args[2].d;
	    var count = this.args[1].d;
	    for ( ; ; count -- ) {
this.dump("REF = "+ref+" "+JSON.stringify(this.param[2]));
		ref = this.env.refer(this,ref);
		if ( count <= 1 ) {
		    this.i_sendResult(ref);
		    return "doFIN_START";
		}
		if ( ref.getType() != "string" ) {
		    var msg = "type missmatch "+this.args[2].getType()+" in refer($) command";
		    this.dump(msg);
		    this.i_sendResult(msg,"error");
		    return "doFIN_START";
		}
		ref = ref.d;
	    }
	}
    }
);


