//:IMPORT ts/tinyState.js
//:IMPORT gt/co_gtObjectAction.js
//:IMPORT std/stdJSON.js
//:IMPORT ts/tsWatchDog.js

function gtObject(parent,gtScope, gtWindow,parameters) {
    if ( arguments.length == 0 )
	return;

    this.uiObject = parameters.uiObject;
    this.gid = parameters.gid;
    this.i_parameters = {};
    if ( parameters.piggybackTurtle ) {
	this.i_parameters.piggybackTurtle
	    = parameters.piggybackTurtle;
    }
    else {
	this.i_parameters.piggybackTurtle = {};
    }
    this.uiObject.gtPane = this;
    var gtWindow = gtWindow || (gtScope ? gtScope.gtWindow : parent.gtWindow );
    if ( !gtScope )
	gtScope = gtWindow;
    this.gtWindow = gtWindow;
    this.gtScope = gtScope;
    this.i_attachList = [];
    this.uiObject.gtPane = this;
    this.i_gtObject_childPane = {};
    if ( !this.rootScope )
	this.rootScope = false;
    tinyState.apply(this,[parent,1]);
    this.initial("gtObject");
}

gtObject.test = false;
gtObject.basePriority = 100;

gtObject.isGtObject = function(className) {
    return stdObject.isClassPrototype(stdObject.classTable[className],"gtObject");
}


gtObject.getUiObjectGtPane = function(uiObject) {
    if ( !uiObject )
	return null;
    if ( uiObject.gtPane )
	return uiObject.gtPane;
    if ( uiObject.parentNode )
	return gtObject.getUiObjectGtPane(uiObject.parentNode);
    return null;
}

stdObject.defClass
(
    "gtObject",
    gtObject,
    tinyState,
    {

	gaction_status : "freeze",

	i_gtObject_childPane : null,

	insertChildPane : function(child) {
	    this.i_gtObject_childPane[child.objId()] = child;
	},

	gaction_freeze : function() {
	    if ( this.state(["FIN","ZOM"]) )
		return;
	    if ( !this.co_gtObjectAction )
		return;
	    this.set_gaction_status("freeze");
	    this.co_gtObjectAction.gaction_freeze();
	},


	enter_gaction : function(caller,state,test) {
	    var parentPane = gtObject.getUiObjectGtPane(this.uiObject.parentNode);
	    if ( !parentPane ) {
		if ( caller.gactionHandle )
		    caller.gactionHandle.remove();
		caller.gactionHandle = null;
		return state;
	    }
	    if ( parentPane.gaction_status == "freeze" ) {
		if ( parentPane.state(["ZOM","FIN"]) ) {
		    if ( caller.gactionHandle )
			caller.gactionHandle.remove();
		    caller.gactionHandle = null;
		    return state;
		}
		if ( !caller.gactionHandle )
		    caller.gactionHandle = parentPane.listen(caller,"state");
/*
if ( test )
this.dump("enter_gaction "+parentPane.objId()+" "+parentPane.gid+" "+parentPane.state()+" "+parentPane.gaction_status);
*/
		return;
	    }
	    if ( parentPane.gaction_status == "noAction" )
		return parentPane.enter_gaction(caller,state,test);
	    if ( caller.gactionHandle )
		caller.gactionHandle.remove();
	    caller.gactionHandle = null;
	    return state;
	},


	i_getChildrenGtList : function(obj) {
	    var ret = [];
	    if ( obj.gtPane )
		ret.push(obj.gtPane);
	    for ( var ix in obj.children ) {
		ret = ret.concat(this.i_getChildrenGtList(obj.children[ix]));
	    }
	    return ret;
	},

	gaction_status_call : false,
	set_gaction_status : function(sts) {
	    this.gaction_status_call = true;
	    if ( !this.co_gtObjectAction ) {
		if ( sts != "noAction" )
		    return;
	    }
	    if ( this.gaction_status == sts )
		return;
	    this.gaction_status = sts;
	    if ( this.gaction_status == "freeze" ) {
		var gtList = this.i_getChildrenGtList(this.uiObject);
		for ( var ix in gtList ) {
		    var target = gtList[ix];
		    if ( target == this )
			continue;
		    target.gaction_freeze();
		}
	    }
	    else {
		this.i_invoke("state");
	    }
	},
	
	priority : function() {
	    if ( !this.uiObject )
		return tinyState.defaultPriority;
	    if ( this.fixPriority ) {
		return this.fixPriority;
	    }
	    var rect = this.uiObject.getBoundingClientRect();
	    var html = document.documentElement;
	    var winY = html.clientHeight;
	    var winX = html.clientWidth;
	    if ( rect.left < winX &&
		 rect.right > 0 &&
		 rect.top < winY &&
		 rect.bottom > 0 ) {
		if ( rect.left < 0 )
		    rect.left = 0;
		if ( rect.top < 0 )
		    rect.top = 0;
		if ( rect.right > winX )
		    rect.right = winX;
		if ( rect.bottom > winY )
		    rect.bottom = winY;
	    }
	    var posX = Math.round((rect.left + rect.right)/2);
	    var posY = Math.round((rect.top + rect.bottom)/2);
	    var priX = Math.round(posX - winX/2);
	    if ( priX < 0 )
		priX = - priX;
	    var priY = Math.round(posY - winY/2);
	    if ( priY < 0 )
		priY = - priY;
	    var ret = priX + priY + gtObject.basePriority;
	    if ( ret >= tinyState.defaultPriority )
		return tinyState.defaultPriority - 1;
	    return ret;
	},
	
	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/

	INI_START : function(ev) {
	    this.gaction = null;
	    return "doINI_gtObject_START";
	},
	INI_gtObject_START : function(ev) {
	    var parentPane = gtObject.getUiObjectGtPane(this.uiObject.parentNode);
	    var penv = piggybackTurtle.top;
	    if ( parentPane ) {
		penv = parentPane.piggybackTurtle;
		parentPane.insertChildPane(this);
	    }
	    else if ( this.gtScope != this ) {
		parentPane = this.gtScope;
		penv = parentPane.piggybackTurtle;
		parentPane.insertChildPane(this);
	    }
	    this.piggybackTurtle = new piggybackTurtle(penv,this);
	    this.piggybackTurtle.bind(
		this,"v.uiObject",
		new pigData("pn:dom",this.uiObject));
	    var ix;
	    var attrs = this.uiObject.attributes;
	    var values = "";
	    if ( attrs ) {
		for ( ix = attrs.length-1 ; ix >= 0 ; ix -- ) {
		    var aa = attrs[ix];
		    this.piggybackTurtle.bind(
			this,
			"a."+aa.name,
			null);
		    if ( aa.value.indexOf("$") == 0 ) {
			if ( aa.value.indexOf(":") >= 0 )
			    values += ',["sq",["wait",["def?","'+aa.value.substr(1)+'"]],["sync",{"a.'+aa.name+'" : "'+aa.value+'"}]]';
			else
			    values += ',["sq",["wait",["def*?","'+aa.value.substr(1)+'"]],["sync",{"a.'+aa.name+'" : "'+aa.value+'"}]]';
			this.uiObject.setAttribute(aa.name,"");
		    }
		}
	    }
	    if ( values != "" )
		this.i_values = values.substr(1);
	    else
		this.i_value = "";
	    var pg = this.i_parameters.piggybackTurtle;
	    for ( ix in pg ) {
		this.piggybackTurtle.bind(
		    this,
		    pg[ix].name,
		    pg[ix].value);
	    }
	    if ( this.gtScope == this )
		return "doINI_gtObject_LAST";
	    this.gtScope.gtPaneInitialCounter --;
	    if ( this.gtScope.gtPaneInitialCounter == 0 )
		this.gtScope.wakeup();
	    this.i_shandle = this.gtScope.listen(this,"state");
	    return "doINI_gtObject_PLUGIN";
	},
	INI_gtObject_PLUGIN : function(ev) {
	    return "doINI_gtObject_LAUNCH";
	},
	INI_gtObject_LAUNCH : function(ev) {
	    if ( this.i_destroyFlag )
		return "doFIN_START";
	    if ( this.gtScope.state(["FIN","ZOM"]) )
		return "doFIN_START";
	    if ( this.gtScope.state(["INI","ACT_KEY"]) )
		return;
	    this.i_shandle.remove();
	    this.i_shandle = null;
	    var gaction_org = this.uiObject.getAttribute("gaction");
	    var gaction_new;
	    if ( this.i_values )
		gaction_new = '["para",'+this.i_values+','+gaction_org+']';
	    else
		gaction_new = gaction_org;
	    this.fixPriority = this.uiObject.getAttribute("gpriority")-0;
	    try {
		this.gaction = stdJSON.parse(gaction_new);
	    }
	    catch ( err ) {
		this.dump("gaction PARSE ERROR CLASS:"+this.className+" GID:"+this.gid+" >> "+err);
		this.gaction = null;
		var line = /line[ \t]+([0-9]+)/g.exec(err);
		if ( line != null ) {
		    var start = line[1]-3;
		    var target = line[1]-0;
		    var last = start+7;
		    if ( start < 1 )
			start = 1;
		    var gary = gaction_org.split(/\n|\r|\r\n/);
		    if ( last > gary.length )
			last = gary.length;
		    var i;
		    var output = "";
		    for ( i = start ; i <= last ; i ++ ) {
			if ( i == start ) {
			    if ( target == i )
				output += ">";
			    else
				output += " ";
			}
			else {
			    if ( target == i )
				output += " >";
			    else
				output += "  ";
			}
			output += stdJSON.strfill(i,4)+" "+gary[i-1];
			if ( i != last )
			    output += "\n";
		    }
		    this.dump(output);
		}
	    }
	    this.gURI = this.uiObject.getAttribute("gURI");
	    if ( this.gaction )
		this.co_gtObjectAction = new co_gtObjectAction(this);
	    return "doINI_gtObject_LAST";
	},
	INI_gtObject_LAST : function(ev) {
	    if ( !this.gaction )
		this.set_gaction_status("noAction");
	    return "doINI_gtObject_END";
	},
	INI_gtObject_END : function(ev) {
	    return "doINI_TINYSTATE_START";
	},


	FIN_START : function(ev) {
	    return "doFIN_gtObject_START";
	},
	FIN_gtObject_START : function(ev) {
	    if ( this.i_shandle )
		this.i_shandle.remove();
	    this.i_shandle = null;
	    if ( !this.co_gtObjectAction )
		return "doFIN_gtObject_PLUGIN";
	    this.co_gtObjectAction.listen(this,"state");
	    this.co_gtObjectAction.destroy();
	    return "doFIN_gtObject_DACTION_RET";
	},
	FIN_gtObject_DACTION_RET : function(ev) {
	    if ( !this.co_gtObjectAction.state("ZOM") ) {
//this.dump("<<"+this.gid+">> gtObject + WAIT "+this.co_gtObjectAction.state());
		if ( !this.watchDog )
		    new tsWatchDog(this);
    		return;
	    }
	    return "doFIN_gtObject_PLUGIN";
	},
	FIN_gtObject_PLUGIN : function(ev) {
	    return "doFIN_gtObject_PLUGIN_FINISH";
	},
	FIN_gtObject_PLUGIN_FINISH : function(ev) {
	    var _ix;
	    for ( _ix in this.i_gtObject_childPane ) {
		var pane = this.i_gtObject_childPane[_ix];
		if ( pane.state("ZOM") )
		    continue;
		pane.listen(this,"state");
		pane.destroy();
	    }
	    return "doFIN_gtObject_START_RET";
	},
	FIN_gtObject_START_RET : function(ev) {
	    if ( this.piggybackTurtle.functionLockCount )
		return;
	    var _ix;
	    for ( _ix in this.i_gtObject_childPane ) {
		var pane = this.i_gtObject_childPane[_ix];
		if ( !pane.state("ZOM") ) {
/*
if ( this.gid == "TRACK" )
this.dump("<<"+this.gid+">> gtObject - WAIT "+pane.className+"("+pane.seq()+") ("+pane.gid+") "+pane.state());
*/
		    return;
		}
		delete this.i_gtObject_childPane[_ix];
	    }
	    this.piggybackTurtle.functionAbort();
	    return "doFIN_gtObject_WAIT_FUNCTION_ABORT";
	},
	FIN_gtObject_WAIT_FUNCTION_ABORT : function(ev) {
	    if ( this.piggybackTurtle.functionLockCount )
		return;
	    this.piggybackTurtle.functionRefUnlock();
	    this.piggybackTurtle.destroy();
	    return "doFIN_gtObject_CLEAR";
	},
	FIN_gtObject_CLEAR : function(ev) {
	    this.i_gtObject_childPane = null;
	    this.co_gtObjectAction = null;
	    this.uiObject.gtPane = null;
	    this.uiObject = null;
	    this.gtScope.detachGtObject(this.gid);
	    this.i_parameters = null;
	    return "doFIN_gtObject_LAST";
	},
	FIN_gtObject_LAST : function(ev) {
	    return "doFIN_TINYSTATE_START";
	},
    }
);
