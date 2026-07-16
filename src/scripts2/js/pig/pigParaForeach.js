//:IMPORT pig/pigPrimitive.js

// ["p-foreach",$ix, $list, action ]

function pigParaForeach(parent,env,param) {
    if (arguments.length == 0)
	return;
    this.env = env;
    if ( param.length > 4 ) {
	this.param = param.concat();
	this.param.length = 4;
    }
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigParaForeach");
}


piggybackTurtle.top.bind(
    null,
    "p-foreach",
    new pigData("pn:primitive",pigParaForeach));

stdObject.defClass
(
    "pigParaForeach",
    pigParaForeach,
    pigPrimitive,
    {


	/*=====================================================
	 *
	 *    pigParaForeach STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    this.args[3] = "*";
	    this.callFunc = new pigActivate(this,this.env,["all"]);
	    return "ACT_START";
	},
	ACT_START : function(ev) {
	    if ( ev.type == "abort" ) {
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.getType() == "error" ) {
		ev.source = this;
		this.parent.eventHandler(ev);
		return "doFIN_START";
	    }
	    if ( this.args[1].getType() != "string" ) {
		this.parent.eventHandler(
		    new stdEvent(
			"return",this,
			new pigData("error","type missmatch string is required (args:1)")));
		return "doFIN_START";
	    }
	    var tag = this.args[1].d;
	    if ( this.args[2].getType() != "array" ) {
		this.i_sendResult(
		    "type missmatch array is required  (args:1)",
		    "error");
		return "doFIN_START";
	    }
	    var list = this.args[2].d;
	    var i;
	    this.nenv = [];
	    this.callBody = [];
	    this.callBodyHash = {};
	    for ( i = 0 ; i < list.length ; i ++ ) {
		var env = new piggybackTurtle(this.env);
		this.nenv[i] = env;
		env.bind(tag,list[i]);
		this.callBody[i] = new pigEval(
		    this,
		    env,
		    this.param[3]);
		this.callBodyHash[this.callBody[i].objId()] = i;
	    }
	    this.ret = [];
	    return "ACT_WAIT";
	},
	ACT_WAIT : function(ev) {
	    if ( ev.type == "abort" ) {
		var i;
		for ( i = 0 ; i < this.callBody.length ; i ++ )
		    this.callBody[i].eventHandler(
			new stdEvent("abort",this));
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    var oid = ev.source.objId();
	    var ix = this.callBodyHash[oid];
	    delete this.callBodyHash[oid];
	    delete this.callBody[ix];
	    delete this.nenv[ix].destroy();
	    if ( ev.msg.getType().indexOf("pn:") == 0 )
		this.ret[ix] = ev.msg;
	    else
		this.ret[ix] = ev.msg.d;
	    if ( this.callBody.length )
		return;
	    this.i_sendResult(this.ret);
	    return "doFIN_START";
	},
	FIN_START : function(ev) {
	    this.callBody = null;
	    this.callBodyHash = null;
	    this.ret = null;
	    return "doFIN_pigPrimitive_START";
	},
    }
);


