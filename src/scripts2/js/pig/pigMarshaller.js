//:IMPORT pig/pigPrimitive.js
//:IMPORT std/stdSemaphore.js
//:IMPORT pig/co_pigMarshaller.js

// ["marshaller",[command-list],[command-list],....]
// command-list
// ["#",   "comment"]
// ["F",   ["input", ....], ["output",....]]
// ["R",   ["input", ....], ["output",....]]
// ["I",                    ["output",....]]
// ["D",                    ["output",....]]
// ["T",   ["tag",....]]

function pigMarshaller(parent,env,param) {
    if (arguments.length == 0)
	return;
    this.i_terminalQueue = [];
    this.i_terminalFilter = [];
    this.i_running = [];
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigMarshaller");
}


piggybackTurtle.top.bind(
    null,
    "marshaller",
    new pigData("pn:primitive",pigMarshaller));

stdObject.defClass
(
    "pigMarshaller",
    pigMarshaller,
    pigPrimitive,
    {

	i_catchAbort : function(ev) {
	    if ( ev.type != "abort" )
		return ev;
	    this.i_abort = true;
	    if ( !this.callFunc )
		return ev;
	    if ( Array.isArray(this.callFunc) ) {
		var i;
		for ( i in this.callFunc )
		    this.callFunc[i].eventHandler(ev);
	    }
	    else if ( this.className == "pigActivate" )
		this.callFunc.eventHandler(ev);
	    else {
		for ( var ix in this.callFunc )
		    this.callFunc[ix].eventHandler(ev);
	    }
	    return ev;
	},

	i_overWrite_one : function(list) {
	    list = list.concat();
	    if ( !this.i_default )
		return list;
	    var dout = this.i_default[1];
	    for ( ix in dout ) {
		if ( list[ix] === "" || list[ix] === null )
		    list[ix] = dout[ix];
	    }
	    return list;
	},

	i_overWrite : function(cmd) {
	    var out;
	    var dout;
	    if ( cmd[0] == "I" ) {
		return ["I",this.i_overWrite_one(cmd[1])];
	    }
	    return [cmd[0],
		    cmd[1].concat(),
		    this.i_overWrite_one(cmd[2])];
	},

	i_launch : function(index,func) {
	    if ( this.i_running[index] )
		return this.i_running[index].set(func);
	    if ( Array.isArray(func) && func.length == 0 )
		return false;
	    this.i_running[index] = new co_pigMarshaller(
		this,
		index,
		this.i_field_output[index],
		func);
	    if ( this.i_field_output[index] )
		return true;
	    return false;
	},
	delRunning : function(index) {
	    delete this.i_running[index];
	},

	i_hitCheck : function(term) {
	    var ix;
	    for ( ix = 0 ; ix < this.i_rule.length ; ix ++  ) {
		var rr_inp = this.i_rule[ix][1];
		var hit_flag = true;
		for ( ixx in rr_inp ) {
		    var dd = rr_inp[ixx];
		    if ( this.i_field_input[ixx] == "" ) {
			if ( this.i_ruleTerminalHandle[ix][ixx] == null )
			    continue;
			if ( term == null ) {
			    hit_flag = false;
			    break;
			}
			if ( this.i_ruleTerminalHandle[ix][ixx] !== term ) {
			    hit_flag = false;
			    break;
			}
			continue;
		    }
		    if ( dd === "" )
			continue;
		    var dd2 = this.env.refer(this,this.i_field_input[ixx]);
		    if ( dd2.getType().indexOf("pn:") == 0 ) {
			hit_flag = false;
			break;
		    }
		    if ( dd2.d != dd ) {
			hit_flag = false;
			break;
		    }
		}
		if ( hit_flag ) {
		    this.i_hitRule = this.i_rule[ix];
		    return this.i_rule[ix];
		}
	    }
	    return null;
	},

	i_set : function(rule) {
	    var rr_out;
	    rr_out = rule[2];
	    var selfUpdated = false;
	    for ( ix in this.i_field_output ) {
		var dd = rr_out[ix];
		if ( dd === "" )
		    continue;
		var upd = false;
		switch ( typeof(dd) ) {
		case "string":
		    if ( dd.indexOf("$") == 0 )
			dd = this.env.refer(this,dd.substr(1));
		    if ( this.i_field_output[ix] ) {
			this.env.bind(this,this.i_field_output[ix],dd);
			upd = true;
		    }
		    break;
		case "object":
		    if ( dd == null )
			break;
		    upd = this.i_launch(ix,dd);
		    break;
		default:
		    if ( this.i_field_output[ix] ) {
			this.env.bind(this,this.i_field_output[ix],dd);
			upd = true;
		    }
		    break;
		}
		if ( upd && this.i_field_input_hash[this.i_field_output[ix]] )
		    selfUpdated = true;
	    }
	    return {
		"selfUpdated" : selfUpdated,
	    };
	},

	i_trans : function(term) {
	    var target;
	    var prev = this.i_target;
	    for ( ; ; ) {
		target =  this.i_hitCheck(term);
		if ( !target )
		    break;
		if ( prev === target )
		    break;
		var status = this.i_set(target);
		if ( !status.selfUpdated )
		    break;
		term = null;
		prev = target;
	    }
	    if ( target )
		this.i_target = target;
	},

	i_terminalEventHandler : function(ev) {
	    if ( this.i_terminalQueue.length == 0 ) {
		this.i_terminalQueue.unshift(ev);
		this.wakeup();
		return;
	    }
	    for ( ix in this.i_terminalFilter ) {
		var sym = this.i_terminalFilter[ix];
		if ( sym == this.i_terminalQueue[0].msg.symbol ) {
		    this.i_terminalQueue.unshift(ev);
		    this.wakeup();
		    return;
		}
	    }
	    this.i_terminalQueue[0] = ev;
	    this.wakeup();
	},

	/*=====================================================
	 *
	 *    pigMarshaller STATE MACHINE
	 *
	 *=====================================================*/


	INI_TINYSTATE_START : function(ev) {
	    this.i_abort = false;
	    this.enableCatchAbort();
	    var share = this.application.refShared("marshaller");
	    if ( share.ref == 1 )
		share.sem = new stdSemaphore(1);

	    var i;
	    for ( i = 1 ; i < this.param.length ; i ++ ) {
		switch ( this.param[i][0] ) {
		case "#":
		    this.args[i] = "*";
		}
	    }
	    this.callFunc = new pigActivate(this,["all",0]);
	    return "INI_MAS_RET";
	},
	INI_MAS_RET : function(ev) {
	    if ( this.i_abort ) {
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
	    if ( this.i_abort ) {
		this.i_setResultEvent("aborted","error");
		return "doFIN_START";
	    }
	    var fix = 1;
	    for ( ; fix < this.param.length ; fix ++ ) {
		if ( this.param[fix][0] == "F" )
		    break;
	    }
	    if ( fix == this.param.length ) {
		var msg = "F (field) list is required";
		this.dump(msg);
		this.i_setResultEvent(msg,"error");
		return "doFIN_START";
	    }
	    this.callFunc = new pigActivate(this,[fix,"all","all"]);
	    return "INI_MAS_RET2";
	},
	INI_MAS_RET2 : function(ev) {
	    if ( this.i_abort ) {
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
	    if ( this.i_abort ) {
		this.i_setResultEvent("aborted","error");
		return "doFIN_START";
	    }
	    var fix = 1;
	    this.callFunc = {};
	    for ( ; fix < this.param.length ; fix ++ ) {
		if ( this.param[fix][0] != "R" )
		    continue;
		var ts = new pigActivate(this,[fix,1,"all"]);
		this.callFunc[ts.objId()] = ts;
	    }
	    return "INI_MAS_RET3";
	},
	INI_MAS_RET3 : function(ev) {
	    if ( this.i_abort ) {
		for ( var ix in this.callFunc )
		    this.callFunc[ix].eventHandler(ev.copy());
		return;
	    }
	    if ( ev.type != "return" )
		return;
	    if ( ev.msg.getType() == "error" ) {
		ev.source = this;
		this.parent.eventHandler(ev);
		return "doFIN_START";
	    }
	    if ( this.i_abort ) {
		this.i_setResultEvent("aborted","error");
		return "doFIN_START";
	    }
	    delete this.callFunc[ev.source.objId()];
	    if ( Object.keys(this.callFunc).length )
		return;
	    var fix = 1;
	    for ( ; fix < this.param.length ; fix ++ ) {
		if ( this.param[fix][0] != "R" )
		    continue;
		this.args[fix][2] = this.param[fix][2];
	    }
	    return "doACT_START";
	},
	ACT_START : function(ev) {
	    return this.application.shared("marshaller").sem
		.get(this,"doACT_START_OK");
	},
	ACT_START_OK : function(ev) {
	    for ( ix in this.param ) {
		if ( ix == 1 )
		    continue;
		switch ( this.param[ix][0] ) {
		case "#":
		case "F":
		case "R":
		    break;
		default:
		    this.args[ix] = this.param[ix];
		}
	    }
	    this.args = pigActivate.normalize(this.args);
	    var i;
	    for ( i = 1 ; i < this.args.length ; i ++ ) {
		var cmd = this.args[i];
		switch ( cmd[0] ) {
		case "#":
		    break;
		case "F":
		    this.i_field_input = cmd[1];
		    this.i_field_output = cmd[2];
		    break;
		case "R":
		    break;
		case "I":
		    this.i_initial = cmd;
		    break;
		case "D":
		    this.i_default = cmd;
		    break;
		case "T":
		    this.i_terminalFilster = cmd[1];
		    break;
		default:
		    this.i_setResultEvent(
			"invalid command marshaller "+cmd[0],
			"error");
		    return "doFIN_FINISH_RELEASE";
		}
	    }
	    if ( this.i_initial )
		this.i_initial = this.i_overWrite(this.i_initial);
	    this.i_rule = [];
	    this.i_ruleTerminalHandle = [];
	    for ( i = 1 ; i < this.args.length ; i ++ ) {
		var cmd = this.args[i];
		if ( cmd[0] != "R" )
		    continue;
		this.i_rule.push(this.i_overWrite(cmd));
	    }
	    this.i_terminalHandle = [];
	    this.i_field_input_hash = {};
	    for ( i = 0 ; i < this.i_field_input.length ; i ++ ) {
		if ( this.i_field_input[i] === "" )
		    continue;
		var dt = this.env.refer(
		    this,
		    this.i_field_input[i],
		    this,
		    this.i_terminalEventHandler);
		if ( dt.getType() == "error" ) {
		    this.i_setResultEvent("cannot refer the symbol ["+this.i_field_input[i]+"]","error");
		    return "doFIN_FINISH_RELEASE";
		}
		this.i_terminalHandle[i] = dt.d;
		this.i_field_input_hash[this.i_field_input[i]] = i;
	    }
	    for ( i = 0 ; i < this.i_rule.length ; i ++ ) {
		this.i_ruleTerminalHandle[i] = [];
		for ( j in this.i_rule[i][1] ) {
		    var symbol = this.i_rule[i][1][j];
		    if ( this.i_field_input[j] != "" )
			continue;
		    if ( symbol === "" )
			continue;
		    if ( typeof(symbol) != "string" || symbol.indexOf("e.") != 0 ) {
			this.i_setResultEvent("cannot refer the not event symbol ["+symbol+"]","error");
			return "doFIN_FINISH_RELEASE";
		    }
		    var dt = this.env.refer(
			this,
			symbol,
			this,
			this.i_terminalEventHandler);
		    if ( dt.getType() == "error" ) {
			this.i_setResultEvent("cannot refer the symbol ["+this.i_rule[i][1][j]+"]","error");
			return "doFIN_FINISH_RELEASE";
		    }
		    this.i_ruleTerminalHandle[i][j] = dt.d;
		}
	    }
	    if ( this.i_initial ) {
		var setup = this.i_initial[1];
		for ( i = 0 ; i < this.i_field_output.length &&
		      i < setup.length ; i ++ ) {
		    var dd;
		    if ( typeof(setup[i]) != "string" || setup[i].indexOf("$") )
			dd = setup[i];
		    else {
			dd = this.env.refer(this,setup[i].substr(1));
			if ( dd.getType() == "error" )
			    continue;
		    }
		    this.env.bind(
			this,
			this.i_field_output[i],
			dd);
		}
	    }
	    this.ret = this.i_trans();
	    return "doACT_FINISH_RELEASE";
	},
	ACT_WAIT : function(ev) {
	    if ( this.i_abort ) {
		this.i_setResultEvent("aborted","error");
		return "doFIN_START";
	    }
	    if ( this.i_terminalQueue.length == 0 )
		return;
	    return "doACT_SEM";
	},
	ACT_SEM : function(ev) {
	    if ( this.i_abort ) {
		this.i_setResultEvent("aborted","error");
		return "doFIN_START";
	    }
	    return this.application.shared("marshaller").sem
		.get(this,"doACT_SEM_OK");
	},
	ACT_SEM_OK : function(ev) {
	    ev = this.i_terminalQueue.pop();
	    this.ret = this.i_trans(ev.msg);
	    return "doACT_FINISH_RELEASE";
	},
	ACT_FINISH_RELEASE : function(ev) {
	    this.application.shared("marshaller").sem.release();
	    return "doACT_FINISH";
	},
	ACT_FINISH : function(ev) {
	    return "doACT_WAIT";
	},

	FIN_FINISH_RELEASE : function(ev) {
	    this.application.shared("marshaller").sem.release();
	    return "doFIN_START";
	},
	FIN_START : function(ev) {
	    for ( var i = 0 ; i < this.i_running.length ; i ++ ) {
		if ( !this.i_running[i] )
		    continue;
		this.i_running[i].listen(this,"state");
		this.i_running[i].set([]);
	    }
	    return "doFIN_WAIT_RUNNING";
	},
	FIN_WAIT_RUNNING : function(ev) {
	    for ( var i = 0 ; i < this.i_running.length ; i ++ ) {
		if ( this.i_running[i] )
		    return;
	    }

	    this.application.unrefShared("marshaller");

	    this.i_terminalQueue = null;
	    this.i_terminalFilter = null;
	    this.i_rule = null;
	    this.i_field_input = null;
	    this.i_field_output = null;
	    this.i_default = null;
	    this.i_initial = null;
	    for ( ix in this.i_terminalHandle )
		if ( this.i_terminalHandle[ix] )
		    this.i_terminalHandle[ix].remove();
	    for ( i in this.i_ruleTerminalHandle )
		for ( j in this.i_ruleTerminalHandle[i] )
		    if ( this.i_ruleTerminalHandle[i][j] )
			this.i_ruleTerminalHandle[i][j].remove();
	    this.i_terminalHandle = null;
	    this.i_ruleTerminalHandle = null;

	    this.i_sendResultEvent();
	    return "doFIN_pigPrimitive_START";
	},
    }
);


