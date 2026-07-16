//:IMPORT gt/gtObject.js
//:EXPORT gtScope


function gtScope(invokerObject, elements, myWindow, parameters) {
    if (arguments.length == 0)
	return;
    
    var gtWindow = null;
    var gtScope = null;
    if ( myWindow == false ) {
        gtWindow = invokerObject.gtWindow;
    }
    else {
        gtWindow = this;
    }
    if ( invokerObject.isClass("gtScope") )
	gtScope = invokerObject;
    else
	gtScope = invokerObject.gtScope;
    gtObject.apply(this,[invokerObject,gtScope,gtWindow,parameters]);

    this.i_lastDestroyDOM = false;
    this.i_elements = elements;
    this.i_childrenWindowList = [];
    this.i_formatNode = {};
    this.i_childrenGtObjectEntryTable = {};
    this.i_childrenNoId = [];
    this.gtPaneInitialCounter = 0;
    this.i_removeDOMlist = {};
    this.initial("gtScope");
}

stdObject.defClass
(
    "gtScope",
    gtScope,
    gtObject,
    {
	i_initializeChildrenList : function(elements) {
	    for (var i = 0; i < elements.length; i++) {
		var ele = elements[i];
		this.i_initializeChildElement(ele);
	    }
	},
	i_initializeChildElement : function(ele,parameters) {
            var className;
	    var result = "nothing";
	    var gid;
            if ( ele.getAttribute ) {
		className = ele.getAttribute("gclass");
		gid = ele.getAttribute("gid") || ele.getAttribute("id");
		if ( gid && !className )
		    className = "gtPane";
	    }
	    else {
		className = null;
		gid = null;
	    }
	    if (className && gid ) {
		if ( stdObject.isClassPrototype(
		    stdObject.classTable[className],
		    "gtObject") )
		    result = "ok";
		
	    }
	    var entry = {};
	    if ( result == "ok" ) {
		entry.element = ele;
		entry.gtPane = null;
		entry.className = className;
		entry.gid = gid;
		if(gid in this.i_childrenGtObjectEntryTable){
		    this.dump("conflict gid error  gid = " + gid);
		}
		this.i_createGtObject(entry,parameters);
		this.i_childrenGtObjectEntryTable[gid] = entry;
	    }
	    else {
		entry.element = ele;
		entry.gtPane = null;
		entry.className = null;
		entry.gid = gid;
		this.i_childrenNoId.unshift(entry);
	    }
	    return entry;
	},
	i_createGtObject : function(entry,parameters) {
    
	    var initFlags = null;

	    this.gtPaneInitialCounter ++;
	    var gtClass = stdObject.classTable[entry.className];
	    var parametersNew = {
		"gid" : entry.gid,
		"uiObject" : entry.element
	    };
	    var ix;
	    for ( ix in parameters ) {
		if ( ix == "gid" )
		    continue;
		if ( ix == "uiObject" )
		    continue;
		parametersNew[ix] = parameters[ix];
	    }
	    var gtobject = new gtClass(this, parametersNew);
    
	    entry.gtPane = gtobject;
	},

	i_makeFormatNode : function() {
	    var fNode = {};
	    var fNodeChildlen = {};
	    this.i_formatNode = {};
	    var elements = this.i_elements;
	    if ( !elements )
		return;
	    for ( var ix = 0 ; ix < elements.length ; ix ++ )
		elements[ix].__formatNodeID = ix;
	    for ( var i = 0 ; i < elements.length ; i ++ ) {
		var target = elements[i];
		if ( !target.getAttribute )
		    continue;
		var gclass = target.getAttribute("gclass");
		if ( !(target.getAttribute("gid") ||
		      target.getAttribute("id")) ||
		     !gclass || gclass.indexOf("*") != 0 )
		    continue;
		if ( fNodeChildlen[target.__formatNodeID] )
		    continue;
		if ( !target.parentNode )
		    continue;
		fNode[target.__formatNodeID] = target;
		var chobj = this.getUiObjectsList(target);
		for ( ix in chobj ) {
		    delete fNode[chobj[ix].__formatNodeID];
		    fNodeChildlen[chobj[ix].__formatNodeID] = chobj;
		}
	    }
	    var elements_ret = [];
	    for ( var ix = 0 ; ix <  elements.length ; ix ++ ) {
		var target = elements[ix];
		if ( fNode[target.__formatNodeID] ) {
		    delete target.__formatNodeID;
		    continue;
		}
		if ( fNodeChildlen[target.__formatNodeID] ) {
		    delete target.__formatNodeID;
		    continue;
		}
		delete target.__formatNodeID;
		elements_ret.push(target);
	    }
	    var fNode_ret = {};
	    for ( ix in fNode ) {
		var target = fNode[ix];
		var ID =  target.getAttribute("gid") || target.getAttribute("id");
		var PID = target.parentNode.getAttribute("gid") ||
		    target.parentNode.getAttribute("id");
		var ele = target.parentNode.removeChild(target);
		if ( PID ) {
		    fNode_ret[PID+"__"+ID] = ele;
		    if ( fNode_ret[ID] )
			fNode_ret[ID] = "dup";
		    else
			fNode_ret[ID] = ele;
		}
		else
		    fNode_ret[ID] = ele;
	    }
	    this.i_elements = elements_ret;
	    this.i_formatNode = fNode_ret;
	},

	getGtObjectRoot : function(astlen) {
	    if ( this.rootScope ) {
		if ( astlen == 0 )
		    return this;
		astlen --;
	    }
	    if ( this.gtScope && this.gtScope != this )
		return this.gtScope.getGtObjectRoot(astlen);
	    return null;
	},

	getGtObjectIdList : function() {
	    var ix;
	    var ret = "";
	    for ( ix in this.i_childrenGtObjectEntryTable ) {
		ret += ","+ix;
	    }
	    if ( ret != "" )
		ret = ret.substr(1);
	    return "["+ret+"]";
	},

	getGtObjectById : function(uiid) {
/*
	    if ( this.state(["ZOM","FIN"]) )
		return null;
*/
	    if ( this.gid == uiid )
		return this;
	    
	    if ( uiid.indexOf("public") == 0 ) {
		var ast = uiid.substr(6);
		if ( ast.match(/\**/) ) {
		    var ret = this.getGtObjectRoot(ast.length);
		    return ret;
		}
	    }
	    if(uiid.indexOf("PARENT_SCOPE:") == 0){
		uiid = uiid.substr("PARENT_SCOPE:".length);
	    }
	    else{
		if ( this.i_childrenGtObjectEntryTable[uiid] ) {
		    if ( this.i_childrenGtObjectEntryTable[uiid].gtPane == null ) {
			this.i_createGtObjects();
		    }
		    return this.i_childrenGtObjectEntryTable[uiid].gtPane;
		}
	    }
	    if (this.gtScope && this.gtScope != this ){
		return this.gtScope.getGtObjectById(uiid);
	    }
	    return null;
	},
	detachGtObject : function(gid) {
	    this.i_childrenGtObjectEntryTable[gid] = null;
	    delete this.i_childrenGtObjectEntryTable[gid];
	},

	getUiObjectsList : function(uiObject) {
	    if ( !uiObject.childNodes )
		return [];
	    var ret = [];
	    var len = uiObject.childNodes.length;
	    for ( var ix = 0 ; ix < len ; ix ++ ) {
		ret.push(uiObject.childNodes[ix]);
		ret = ret.concat(this.getUiObjectsList(uiObject.childNodes[ix]));
	    }
	    return ret;
	},

	getFormatDOM : function(gid,pgid) {
	    if ( this.state(["ZOM","FIN"]) )
		return null;
	    if ( pgid ) {
		var nd = this.i_formatNode[pgid+"__"+gid];
		if ( nd )
		    return nd;
	    }
	    var nd = this.i_formatNode[gid];
	    if ( nd != "dup" && nd )
		return nd;
	    if ( this.gtScope && this.gtScope != this )
		return this.gtScope.getFormatDOM(gid);
	    return null;
	},

	makeDOM : function(uiObject,text) {
	    uiObject.innerHTML = text;
	    this.i_elements = this.getUiObjectsList(uiObject);
	    this.i_makeFormatNode();
	    this.i_initializeChildrenList(this.i_elements);
	},
	appendDOMformat : function(uiObject,pos,nd,parameters) {
	    nd = nd.cloneNode(true);
	    uiObject.insertBefore(nd,pos);
	    nd.setAttribute("gid",parameters.gid);
	    var gclass = nd.getAttribute("gclass");
	    var paneAction = "top";
	    if ( gclass != "*format" && gclass != "*" ) {
		gclass = gclass.substr(1);
		var gtClass = stdObject.classTable[gclass];
		if ( !gtClass ) {
		    this.dump("appendDOMformat : invalid class "+gclass);
		    paneAction = "all";
		}
		else if ( !stdObject.isClassPrototype(gtClass,"gtScope") )
		    paneAction = "all";
		else {
		    nd.setAttribute("gclass",gclass);
		}
	    }
	    else {
		nd.setAttribute("gclass","gtScopeNode");
	    }
	    this.i_initializeChildElement(nd,parameters);
	    if ( paneAction == "all" ) {
		var elements = this.getUiObjectsList(nd);
		for ( ix in elements ) {
		    var ele = elements[ix];
		    this.i_initializeChildElement(ele);
		}
	    }
	    return nd;
	},

	i_removeDOM_handler : function(ev) {
	    var sid = ev.source.objId();
	    var pane = this.i_removeDOMlist[sid];
	    if ( !pane )
		return;
	    if ( !pane.gtPane.state("ZOM") )
		return;
	    pane.uiObject.parentNode.removeChild(pane.uiObject);
	    if ( pane.handler && pane.caller )
		pane.handler.apply(pane.caller,[ev]);
	    delete this.i_removeDOMlist[sid];

	    if ( Object.keys(this.i_removeDOMlist).length == 0 )
		this.wakeup();
	},

	removeDOM : function(uiObject,caller,handler) {
	    if ( !uiObject.gtPane )
		return;
	    var pane = uiObject.gtPane;
	    var sid = pane.objId();
	    if ( this.i_removeDOMlist[sid] )
		return;
	    pane.listen(this,"state",this.i_removeDOM_handler);
//	    this.dump("remove "+pane.className+" "+pane.objId()+" "+pane.gid);
	    this.i_removeDOMlist[sid] = {
		"gtPane" : pane,
		"uiObject" : uiObject,
		"caller" : caller,
		"handler" : handler
	    };
	    pane.destroy();
	},
	destroyDOM : function(clearInner) {
	    for ( gid in this.i_childrenGtObjectEntryTable ) {
		var pane = this.i_childrenGtObjectEntryTable[gid].gtPane;
		pane.listen(this,"state");
/*
if ( pane.gid == "TRACK" )
this.dump("TRACK "+pane.objId()+" IS DESTROYED BY "+this.gid+" P="+this.parent.gid);
*/
		pane.destroy();
	    }
	    this.i_clearInner = clearInner;
	},
	waitDestroyDOM : function(nextState) {
	    for ( gid in this.i_childrenGtObjectEntryTable ) {
		var pane = this.i_childrenGtObjectEntryTable[gid].gtPane;
		if ( !pane.state("ZOM") ) {

this.dump("<<"+this.gid+">> ZOM WAIT "+pane.className+"("+pane.seq()+") ("+pane.gid+") "+pane.state());

		    pane.listen(this,"destroyed");
		    return;
		}
	    }
	    if ( this.i_clearInner )
		this.uiObject.innerHTML = "";
	    return nextState;
	},
	getChildrenCount : function() {
	    var ret = 0;
	    for ( ix in this.i_childrenGtObjectEntryTable )
		ret ++;
	    return ret;
	},

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/

	INI_gtObject_END : function(ev) {
	    if ( this.i_elements ) {
		this.i_makeFormatNode();
		this.i_initializeChildrenList(this.i_elements);
	    }
	    return "doINI_gtScope_LAUNCH_PANE";
	},
	INI_gtScope_LAUNCH_PANE : function(ev) {
	    if ( this.gtPaneInitialCounter != 0 )
		return;
	    return "doINI_gtScope_END";
	},
	INI_gtScope_END : function(ev) {
	    return "doINI_TINYSTATE_START";
	},

	FIN_START : function(ev) {
	    return "doFIN_gtScope_START";
	},
	FIN_gtScope_START : function(ev) {
	    if ( Object.keys(this.i_removeDOMlist).length )
		return;
	    return "doFIN_gtObject_START";
	},
	
	FIN_gtObject_PLUGIN : function(ev) {
	    this.destroyDOM(this.i_lastDestroyDOM);
	    return "doFIN_gtScope_WAIT";
	},
	FIN_gtScope_WAIT : function(ev) {
	    return this.waitDestroyDOM("doFIN_gtScope_LAST");
	},
	FIN_gtScope_LAST : function(ev) {
	    this.i_childrenGtObjectEntryTable = null;
	    this.i_formatNode = null;
	    this.i_elements = null;
	    return "doFIN_gtObject_PLUGIN_FINISH";
	},
    }
);
