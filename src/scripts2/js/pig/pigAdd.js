//:IMPORT pig/pigPrimitive.js

function pigAdd(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigAdd");
}

piggybackTurtle.top.bind(
    null,
    "add",
    new pigData("pn:primitive",pigAdd));

piggybackTurtle.top.bind(
    null,
    "+",
    new pigData("pn:primitive",pigAdd));


stdObject.defClass
(
    "pigAdd",
    pigAdd,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigAdd STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.i_abort = false;
	    this.callFunc = new pigActivate(this,["all"]);
	    return "ACT_START";
	},
	ACT_START : function(ev) {
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
	    if ( this.args.length == 1 ) {
		this.i_sendResult(0);
		return "doFIN_START";
	    }
	    for ( i = 1 ; i < this.args.length ; i ++ ) {
		if ( this.args[i].getType() == "error" ) {
		    this.i_sendResult(this.args[i]);
		    return "doFIN_START";
		}
	    }
	    this.args = pigActivate.normalize(this.args);
	    var ret = this.args[1];
	    var i;
	    for ( i = 2 ; i < this.args.length ; i ++ ) {
		ret += this.args[i];
	    }
	    this.i_sendResult(ret);
	    return "doFIN_START";
	},
    }
);


