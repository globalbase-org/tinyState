//:IMPORT ts/tinyState.js
//:IMPORT ts/tsWatchDog.js

function co_gtObjectAction(parent) {
    if (arguments.length == 0)
	return;
    tinyState.apply(this, [parent]);
    this.initial("co_gtObjectAction");
}

stdObject.defClass
(
    "co_gtObjectAction",
    co_gtObjectAction,
    tinyState,
    {

	i_freeze_flag : false,

	gaction_freeze : function() {
	    this.i_freeze_flag = true;
	    this.parent.set_gaction_status("freeze");
	    this.wakeup();
	},

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/


	INI_START : function(ev) {
	    if ( !this.parent.gURI )
		return "doINI_ACTION";
	    return "doINI_START_gURI";
	},
	INI_START_gURI : function(ev) {
	    var sp_uri = this.parent.gURI.split(/[ \t]*,[ \t]*/);
	    this.target = [];
	    for ( ix in sp_uri ) {
		var guri = sp_uri[ix];
		if ( guri.indexOf("$",0) == 0 ) {
		    guri = this.parent.piggybackTurtle.refer(this,guri);
		    if ( guri.getType() == "error" ) {
			this.dump("cannot access "+sp_uri[ix]);
			continue;
		    }
		    guri = guri.d;
		}
		this.parent.piggybackTurtle.bind(this,"g."+guri);
		var gt = this.application.getGtItem(guri);
		gt.listen(this,"destroyed");
		this.target.push({
		    "gt" : gt,
		    "uri" : guri
		});
	    }
	    return "doACT_RESTART_CHECK";
	},
	INI_ACTION : function(ev) {
	    this.i_action_handle = this.parent.listen(this,"state");
	    return "doINI_LAUNCH_ACTION";
	},
	
	INI_LAUNCH_ACTION : function(ev) {
	    if ( this.parent.state(["FIN","ZOM"]) )
		return "doFIN_ACTION_FINISH";
	    if ( this.parent.state(["INI"]) )
		return;
	    if ( this.i_action_handle ) {
		this.i_action_handle.remove();
		this.i_action_handle = null;
	    }
//this.dump("co_gtObjectAction = "+this.parent.className+" "+JSON.stringify(this.parent.gaction));
	    this.i_freeze_flag = 0;
	    return "doINI_LAUNCH_ACTION_WAIT";
	},
	INI_LAUNCH_ACTION_WAIT : function(ev) {
	    return this.parent.enter_gaction(this,"doINI_LAUNCH_ACTION_GO");
	},
	INI_LAUNCH_ACTION_GO : function(ev) {
	    if ( this.i_destroyFlag )
		return "doFIN_START";
	    this.parent.gaction_status_call = false;
	    this.pigEvalPtr = new pigEval(
		this,
		this.parent.piggybackTurtle,
		this.parent.gaction);
	    if ( !this.parent.gaction_status_call )
		this.parent.set_gaction_status("act");
	    return "ACT_START";
	},

	ACT_TINYSTATE_START : function(ev) {
	    if ( ev.type == "return" ) {
		if ( ev.msg.getType() == "error" )
		    this.dump("["+this.parent.gid+"] gaction ERROR "+ev.msg.d);
		this.parent.set_gaction_status("act");
		return "doACT_SUSPEND";
	    }

	    if ( ev.type == "destroyed" || this.i_freeze_flag )
		return "doACT_RESTART";
/*
	    if ( ev.type == "destroyed" ) {
		this.dump("RESTART DESTROYED");
		return "doACT_RESTART";
	    }
	    if ( this.i_freeze_flag ) {
		this.dump("RESTART FREEZE");
		return "doACT_RESTART";
	    }
*/
	    return "ACT_START";
	},
	ACT_SUSPEND : function(ev) {
	    if ( this.i_destroyFlag )
		return "doFIN_START";
	    if ( ev.type == "destroyed" )
		return "doACT_RESTART_CHECK";
	    if ( this.i_freeze_flag )
		return "doACT_RESTART_CHECK";
	    return;
	},
	ACT_RESTART : function(ev) {
	    this.parent.set_gaction_status("freeze");
	    this.pigEvalPtr.abort();
	    return "ACT_RESTART_RET";
	},
	ACT_RESTART_RET : function(ev) {
	    if ( this.i_destroyFlag )
		return "doFIN_START";
	    if ( ev.type != "return" )
		return;
	    this.pigEvalPtr = null;
	    return "doACT_RESTART_CHECK";
	},
	ACT_RESTART_CHECK : function(ev) {
	    this.i_target = null;
	    for ( ix in this.target ) {
		var tbl = this.target[ix];
		if ( tbl.gt.state(["ZOM","FIN"]) ) {
		    this.i_target = tbl;
		    return "doACT_RESTART_WATCH";
		}
	    }
	    return "doINI_ACTION";
	},
	ACT_RESTART_WATCH : function(ev) {
	    return "doACT_RESTART_DESTROY";
	},
	ACT_RESTART_DESTROY : function(ev) {
	    this.i_clist = this.application.getGtItemChildren(this.i_target.uri);
	    for ( var uu in this.i_clist ) {
/*
this.dump("STOP = "+this.i_clist[0].i_URI+" "+this.i_clist[0].state());
this.trace("ME");
*/
		this.i_clist[uu].listen(this,"state");
		this.i_clist[uu].destroy();
	    }
	    return "doACT_RESTART_WAIT";
	},
	ACT_RESTART_WAIT : function(ev) {
	    for ( var uu in this.i_clist ) {
		if ( !this.i_clist[uu].state("ZOM") )
{
/*
this.dump("WAIT = "+this.i_clist[0].i_URI+" "+this.i_clist[0].state());
*/
                     return;
}
	    }
	    this.i_target.gt = this.application.getGtItem(this.i_target.uri);
	    if ( !this.i_target.gt.state(["FIN","ZOM"]) ) {
		this.parent.piggybackTurtle.bind(this,"g."+this.i_target.uri);
		var gt = this.application.getGtItem(this.i_target.uri);
		this.i_target.gt = gt;
		this.i_target.gt.listen(this,"destroyed");
		return "doACT_RESTART_CHECK";
	    }
	    stdInterval.wait(ifThis,1000000,"timer");
	    return "ACT_RESTERT_TIMER";
	},
	ACT_RESTART_TIMER : function(ev) {
	    return "doACT_RESTART_CHECK";
	},


	FIN_START : function(ev) {
	    if ( !this.pigEvalPtr || this.pigEvalPtr.state(["ZOM"]) )
		return "doFIN_TINYSTATE_START";
	    this.pigEvalPtr.listen(this,"state");
	    this.pigEvalPtr.abort();
	    return "doFIN_START_RET";

	},
	FIN_START_RET : function(ev) {
	    if ( ev.type == "return" && ev.msg.getType() == "error" && ev.msg.d != "aborted" ) {
		this.dump("["+this.parent.gid+"] gaction ERROR "+ev.msg.d);
		return;
	    }
	    if ( !this.pigEvalPtr.state(["ZOM"]) ) {
		if ( !this.watchDog )
		    new tsWatchDog(this);
		return;
	    }
	    return "doFIN_ACTION_FINISH";
	},
	FIN_ACTION_FINISH : function(ev) {
	    this.pigEvalPtr = null;
	    return "doFIN_TINYSTATE_START";
	},
    }
);
