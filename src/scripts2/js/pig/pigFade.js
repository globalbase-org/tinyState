//:IMPORT pig/pigPrimitive.js
//:IMPORT pig/pigFadeInvoker.js

// ["fade",variable,[options],duration,targetData]
// options
// ["type","floating"/"integer"]
// ["cycle",number]

function pigFade(parent,env,param) {
    if (arguments.length == 0)
	return;
    pigPrimitive.apply(this, [parent,env,param]);
    this.initial("pigFade");
}

piggybackTurtle.top.bind(
    null,
    "fade",
    new pigData("pn:primitive",pigFade));


stdObject.defClass
(
    "pigFade",
    pigFade,
    pigPrimitive,
    {

	/*=====================================================
	 *
	 *    pigFade STATE MACHINE
	 *
	 *=====================================================*/

	INI_TINYSTATE_START : function(ev) {
	    this.enableCatchAbort();
	    this.callFunc = new pigActivate(this,[2,"all"]);
	    return "INI_ARGS_2";
	},
	INI_ARGS_2 : function(ev) {
	    if ( ev.type != "return" )
		return;
	    if ( this.i_abort ) {
		this.i_sendResult("aborted","error");
		return "doFIN_START";
	    }
	    this.callFunc = new pigActivate(this,["all"]);
	    return "INI_ARGS_3";
	},
	INI_ARGS_3 : function(ev) {
	    if ( ev.type != "return" )
		return;
	    if ( this.i_abort ) {
		this.i_sendResult("aborted","error");
		return "doFIN_START";
	    }
	    this.callFunc = null;

	    this.args = pigActivate.normalize(this.args);

	    this.i_type = "floating";
	    this.i_cycle = 20000;
	    var i;
	    for ( i = 0 ; i < this.args[2].length ; i ++ ) {
		var el = this.args[2][i];
		var sym = el[0];
		switch ( sym ) {
		case "type":
		    switch ( el[1] ) {
		    case "floating":
		    case "integer":
			this.i_type = el[1];
			break;
		    default:
			this.i_sendResult("invalid type "+el[1],
					  "error");
			return "doFIN_START";
		    }
		    break;
		case "cycle":
		    if ( typeof ( el[1] ) != "number" ) {
			this.i_sendResult("type missmatch "+el[1]+
					  " number is required for cycle",
					  "error");
			return "doFIN_START";
		    }
		    this.i_cycle = el[1];
		    break;
		}
	    }
	    this.i_variable = this.args[1];
	    this.i_duration = this.args[3];
	    this.i_targetData = this.args[4];
	    this.i_value = this.env.refer(
		this,
		this.i_variable);
	    this.i_term = this.i_value.terminal;
	    switch ( this.i_value.getType() ) {
	    case "error":
		this.i_sendResult(this.i_value);
		return "doFIN_START";
	    case "string":
		this.i_valueStart = this.i_value.d-"0";
		break;
	    case "number":
		this.i_valueStart = this.i_value.d;
		break;
	    default:
		this.i_sendResult("type missmatch the arg 2 "+
				 this.i_value.getType(),
				 "error");
		return "doFIN_START";
	    }
	    if ( typeof(this.i_duration) != "number" ) {
		this.i_sendResult("type missmatch the arg 4 "+
				 typeof(this.i_duration),
				 "error");
		return "doFIN_START";
	    }
	    if ( typeof(this.i_targetData) != "number" ) {
		this.i_sendResult("type missmatch the arg 5 "+
				 typeof(this.i_duration),
				 "error");
		return "doFIN_START";
	    }
	    if ( this.i_type == "floating" ) 
		return "doINI_FLOATING";
	    return "doINI_INTEGER";
	},
	INI_FLOATING : function(ev) {
	    var div = this.i_targetData - this.i_valueStart;
	    if ( div == 0 )
		return "doACT_WAIT_DURATION";
	    this.i_countTotal = Math.ceil(this.i_duration / this.i_cycle);
	    this.i_pitch = div / this.i_countTotal;
	    this.i_counter = 0;

	    var shared = this.application.refShared("pigFade");
	    if ( !shared.fade )
		shared.fade = [];
	    var master = shared[this.i_cycle];
	    if ( !master )
		master = new pigFadeInvoker(this.application,this.i_cycle);
	    master.listen(this,"updated");
	    return "ACT_FLOATING";
	},
	ACT_FLOATING : function(ev) {
	    if ( this.i_abort )
		return "doACT_FLOATING_FINISH";
	    if ( ev.type != "updated" )
		return;
	    this.i_counter ++;
	    if ( this.i_counter == this.i_countTotal )
		this.i_value = this.i_targetData;
	    else
		this.i_value = this.i_pitch * this.i_counter + this.i_valueStart;
	    this.i_term.parent.bind(
		this,
		this.i_variable,
		new pigData("pure",this.i_value));
	    if ( this.i_counter == this.i_countTotal )
		return "doACT_FLOATING_FINISH";
	    return;
	},
	ACT_FLOATING_FINISH : function(ev) {
	    this.application.unrefShared("pigFade");
	    return "doACT_FINISH";
	},


	INI_INTEEGER : function(ev) {
	    this.i_targetData = Math.floor(this.i_targetData);
	    this.i_valueStart = Math.floor(this.i_valueStart);
	    this.i_value = this.i_value;
	    this.i_div = this.i_targetData - this.i_valueStart;
	    if ( this.i_div == 0 )
		return "doACT_WAIT_DURATION";
	    if ( this.i_div > 0 )
		this.i_cycle = this.i_duration/this.i_div;
	    else
		this.i_cycle = this.i_duration/(-this.i_div);
	    return "doACT_INTEGER_WAIT";
	},
	ACT_INTEGER_WAIT : function(ev) {
	    stdInterval.wait(this,this.i_cycle,"return");
	    return "ACT_INTEGER_WAIT_RET";
	},
	ACT_INTEGER_WAIT_RET : function(ev) {
	    if ( this.i_abort )
		return "doACT_FINISH";
	    if ( ev.type != "return" )
		return;
	    if ( this.i_div > 0 )
		this.i_value ++;
	    else 
		this.i_value --;
	    this.i_term.parent.bind(
		this,
		this.i_variable,
		new pigData("pure",this.i_value));
	    if ( this.i_value == this.i_targetData )
		return "doACT_FINISH";
	    return "doACT_INTEGER_WAIT";
	},

	ACT_WAIT_DURATION : function(ev) {
	    stdInterval.wait(this,this.i_duration,"return");
	    return "ACT_WAIT_DURATION_RET";
	},
	ACT_WAIT_DURATION_RET : function(ev) {
	    if ( this.i_abort )
		return "doACT_FINISH";
	    if ( ev.type != "return" )
		return;
	    return "doACT_FINISH";
	},

	ACT_FINISH : function(ev) {
	    if ( this.i_abort )
		this.i_sendResult("aborted","error");
	    else
		this.i_sendResult(this.i_value);
	    return "doFIN_START";
	},
    }
);


