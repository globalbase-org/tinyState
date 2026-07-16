//:IMPORT pig/pigParaGate.js
//:IMPORT pig/pigPrimitive.js

function pigPara(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigPara");
}


piggybackTurtle.top.bind(
    null,
    "para",
    new pigData("pn:primitive",pigPara));


pigPara.paraGate = function(caller) {
    prev = null;
    for ( ; caller ; ) {
	if ( caller.className == "pigPara" )
	    break;
	prev = caller;
	caller = caller.parent;
    }
    if ( !caller )
	return -1;
    return caller.i_doParaGate(prev);
}

stdObject.defClass
(
    "pigPara",
    pigPara,
    pigPrimitive,
    {

	i_paraGate : 0,
	i_paraGateFinish : 0,
	i_paraGateRecv : false,
	i_sendAbort : false,

	i_doParaGate : function(target) {
	    var i;
	    if ( this.i_paraGateRecv )
		return 1;
	    this.i_paraGateRecv = true;
	    for ( i = this.callFunc.length-1 ; i >= 1 ; i -- ) {
		if ( this.callFunc[i] !== target )
		     continue;
		this.i_paraGate = i;
		this.wakeup();
		return 0;
	    }
	    this.i_paraGate = -1;
	    this.wakeup();
	    return 0;
	},

	/*=====================================================
	 *
	 *    pigPara STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    this.enableCatchAbort();
	    this.callFunc = [];
	    this.i_return_count = 1;
	    this.i_returnEvent = [];
	    var i;
	    for ( i = 1 ; i < this.param.length ; i ++ ) {
		this.callFunc[i] = new pigEval(this,this.env,this.param[i]);
		if ( this.i_paraGate < 0 )
		    this.i_paraGate = i;
	    }
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( ev.type != "return" ) {
		if ( this.i_paraGate )
		    return "doACT_SEND_ABORT";
		return;
	    }
	    var i;
	    for ( i = this.param.length - 1 ; i >= 1 ; i -- )
		if ( this.callFunc[i] === ev.source ) {
		    this.i_returnEvent[i] = ev.msg;
		    this.i_return_count ++;
		    break;
		}
	    if ( this.i_return_count == this.param.length )
		return "doFIN_START";
	    if ( ev.msg.getType() == "error" )
		return "doACT_SEND_ABORT";
	    return;
	},
	ACT_SEND_ABORT : function(ev) {
	    if ( this.i_sendAbort )
		return "ACT_START";
	    var i;
	    for ( i = this.param.length-1 ; i >= 1 ; i -- ) {
		if ( this.i_returnEvent[i] )
		    continue;
		if ( this.i_paraGate == i )
		    continue;
		this.callFunc[i].abort();
	    }
	    this.i_sendAbort = true;
	    if ( this.i_paraGate ) {
		this.i_paraGateFinish
		    = this.i_paraGate;
		this.i_paraGate = 0;
	    }
	    return "ACT_START";
	},
	FIN_START : function(ev) {
	    if ( this.i_paraGateFinish ) {
		this.i_sendResult(this.i_returnEvent[this.i_paraGateFinish]);
		return "doFIN_PARA_FINISH";
	    }
	    var i;
	    var ret = [];
	    for ( i = 1 ; i < this.callFunc.length ; i ++ ) {
		var v = this.i_returnEvent[i];
		if ( v.getType() == "error" && v.d != "aborted" ) {
		    this.i_sendResult(v.d,"error");
		    return "doFIN_PARA_FINISH";
		}
		ret.push(v.d);
	    }
	    if ( this.i_abort ) {
		this.i_sendResult("aborted","error");
		return "doFIN_PARA_FINISH";
	    }
	    this.i_sendResult(ret);
	    return "doFIN_PARA_FINISH";
	},
	FIN_PARA_FINISH : function(ev) {
	    return "doFIN_TINYSTATE_START";
	},
    }
);


