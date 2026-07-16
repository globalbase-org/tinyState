//:IMPORT pig/pigPrimitive.js

/*
["gtw","gtValue"
  ["...","...",....,data],
  ["...","...",....,data],
  ....]
*/


function pigGtiWrite(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigGtiWrite");
}

piggybackTurtle.top.bind(
    null,
    "gtiWrite",
    new pigData("pn:primitive",pigGtiWrite));

stdObject.defClass
(
    "pigGtiWrite",
    pigGtiWrite,
    pigPrimitive,
    {

	i_abort : false,

	/*=====================================================
	 *
	 *    pigGtiWrite STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.callFunc = new pigActivate(this,[1]);
	    return "doINI_RET";
	},
	INI_RET : function(ev) {
	    if ( ev.type == "abort" ) {
		this.i_abort = true;
		this.callFunc.eventHandler(ev);
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    if ( this.i_abort ) {
		this.i_sendResult("error","aborted");
		return "doFIN_START";
	    }
	    this.callFunc = new pigActivate(this,["all","all"]);
	    return "INI_RET_2";
	},
	INI_RET_2 : function(ev) {
	    if ( ev.type == "abort" ) {
		this.callFunc.eventHandler(ev);
		this.i_abort = true;
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    if ( this.i_abort ) {
		this.i_sendResult("error","aborted");
		return "doFIN_START";
	    }
	    return "doACT_START";
	},
	ACT_START : function(ev) {
	    var gt_args = [];
	    this.args = pigActivate.normalize(this.args);
	    var command_default = "write";
	    for ( i = 2 ; i < this.args.length ; i ++ ) {
		var item = this.args[i];
		if ( item.length == 0 )
		    continue;
		var path = "";
		for ( j = 0 ; j < item.length-1 ; j ++ ) {
		    path += item[j];
		}
		var point = path.indexOf("..");
		if ( point >= 0 )
		    path = path.substr(point+2);
		if ( path == "command" )
		    command_default = "";
		gt_args.push({
		    "path" : path.split("."),
		    "data" : item[j]
		});
	    }
	    if ( command_default != "" ) {
		gt_args.push({
		    "path" : ["command"],
		    "data" : "write"
		});
	    }
	    var gt = this.args[1];
	    if ( typeof(gt) != "string" ) {
		this.i_sendResult("type missmatch arg 1 (string g type variable name is required)","error");
		return "doFIN_START";
	    }
	    if ( gt.indexOf("g.") == 0 )
		gt = gt.substr(2);
	    else if ( gt.indexOf(gtPolling.URIheader) == 0 )
		gt = gt.substr(gtPolling.URIheader.length);
	    var point = gt.indexOf("..");
	    if ( point >= 0 )
		gt = gt.substr(0,point);
	    gt = this.application.getGtItem(gt);
	    gt.write(this,gt_args);
	    return "doACT_PREV_RET";
	},
	ACT_PREV_RET : function(ev) {
	    return "ACT_RET";
	},
	ACT_RET : function(ev) {
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.status == "error" ) {
		this.i_sendResult(ev.msg.status2,"error");
		return "doFIN_START";
	    }
	    this.i_sendResult(ev.msg.post);
	    return "doFIN_START";
	}
    }
);


