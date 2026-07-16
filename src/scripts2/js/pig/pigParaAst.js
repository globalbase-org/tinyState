//:IMPORT pig/pigPrimitive.js

function pigParaAst(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigParaAst");
}


piggybackTurtle.top.bind(
    null,
    "para*",
    new pigData("pn:primitive",pigParaAst));

stdObject.defClass
(
    "pigParaAst",
    pigParaAst,
    pigPrimitive,
    {

	i_abort_count : 0,

	/*=====================================================
	 *
	 *    pigParaAst STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    this.callFunc = [];
	    var i;
	    for ( i = 1 ; i < this.param.length ; i ++ )
		this.callFunc[i] = new pigEval(this,this.env,this.param[i]);
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    this.i_abort = false;
	    if ( ev.type == "abort" ) {
		var i;
		for ( i = 1 ; i < this.callFunc.length ; i ++ )
		    this.callFunc[i].eventHandler(ev);
		this.i_abort = true;
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    this.i_returnEvent = ev;
	    this.i_abort_count = 2;
	    if ( this.i_abort )
		return "ACT_WAIT_ABORT";
	    var abort_ev = new stdEvent("abort",this);
	    var i;
	    for ( i = 1 ; i < this.callFunc.length ; i ++ ) {
		if ( this.callFunc[i] === ev.source )
		    continue;
		this.callFunc[i].eventHandler(abort_ev);
	    }
	    return "ACT_WAIT_ABORT";
	},
	ACT_WAIT_ABORT : function(ev) {
	    if ( ev.type != "return" )
		return;
	    this.i_abort_count ++;
	    if ( this.i_abort_count < this.callFunc.length )
		return;
	    this.i_returnEvent.source = this;
	    this.parent.eventHandler(this.i_returnEvent);
	    return "doFIN_START";
	}
    }
);


