//:IMPORT std/stdObject.js
//:IMPORT pig/pigTerminalHandle.js

function pigTerminal(parent,caller,symbol,path,data) {
    if (arguments.length == 0)
	return;
    this.parent = parent;
    if ( data.getType() == null )
	this.data = data;
    else
	this.data = (new pigData("pure",{})).set(path,data);
    this.symbol = symbol;
    this.listenerList = {};
    if ( this.symbol.indexOf("e.") == 0 ) {
	var pigThis = this;
	this.parent.gtObject.uiObject.addEventListener(
	    this.symbol.substr(2),
	    function(event) {
		pigThis.set(
		    null,
		    "",
		    new pigData("pn:domEvent",event));
	    },
	    true);
    }
    else if ( this.symbol.indexOf("g.") == 0 ) {
	var host = this.parent.getHost().application;
	var pigThis = this;
	if ( caller && caller.isClass("gtItem") )
	    this.gtItem = caller;
	else
	    this.gtItem = host.application.getGtItem(symbol.substr(2));
	this.gtItemHandle 
	    = this.gtItem.listen(host,"updated",function(ev) {
		pigThis.update(ev.ignore);
	})
	this.gtItem.addRef(this.parent);
    }
    else if ( this.symbol.indexOf("a.") == 0 ) {
	if (  data.getType() == "null" )
	    this.set(this.parent.host,
		     "",
		     new pigData(
			 "pure",
			 this.parent.gtObject.uiObject.getAttribute(
			     this.symbol.substr(2))),true);
	else
	    this.parent.gtObject.uiObject.setAttribute(
		this.symbol.substr(2),
		data.d);
    }
    else if ( this.symbol.indexOf("p.") == 0 ) {
	if (  data.getType() == "null" )
	    this.set(this.parent.host,
		     "",
		     new pigData(
			 "pure",
			 this.parent.gtObject.uiObject[this.symbol.substr(2)]),true)
	else {
	    if ( path == "" || path.indexOf(".") >= 0 )
		this.parent.gtObject.uiObject[this.symbol.substr(2)] = data.d;
	    else
		this.parent.gtObject.uiObject[this.symbol.substr(2)][path] = data.d;
	}
    }
    stdObject.apply(this);
}

pigTerminal.getStyleProperty = function(ele,prop) {
    if(ele.currentStyle){
	return ele.currentStyle[prop]; //IE
    }
    else{
	var style =  document.defaultView.getComputedStyle(ele, null)　//firefox, Operaなど
	return style.getPropertyValue(prop);
    }
}

stdObject.defClass
(
    "pigTerminal",
    pigTerminal,
    stdObject,
    {

	set : function(caller,path,data,handlerFlag) {
	    if ( !this.symbol )
		return;
	    if ( data.getType() == "null" )
		return;
	    var t1 = this.data.getType();
	    var t2 = data.getType();
	    if ( path == "" && t1 == t2 && t1.indexOf("pn:") != 0 ) {
		t1 = typeof(this.data.d);
		if ( t1 != "object" && this.data.d == data.d )
		    return;
	    }
	    this.data.set(path,data);
	    if ( !handlerFlag && this.symbol.indexOf("a.") == 0 ) {
		if ( path != "" || 
		     typeof(data.d) == "undefined" ||
		     data.d === null )
		    this.parent.gtObject.uiObject.setAttribute(
			this.symbol.substr(2),
			"");
		else
		    this.parent.gtObject.uiObject.setAttribute(
			this.symbol.substr(2),
			data.d);
	    }
	    else if ( !handlerFlag && this.symbol.indexOf("p.") == 0 ) {
		if ( path == "" || path.indexOf(".") >= 0 ) {
		    if ( typeof(data.d) == "undefined" ||
			 data.d === null )
			this.parent.gtObject.uiObject[this.symbol.substr(2)] = "";
		    else
			this.parent.gtObject.uiObject[this.symbol.substr(2)] = data.d;
		}
		else {
		    if ( typeof(data.d) == "undefined" ||
			 data.d === null )
			this.parent.gtObject.uiObject[this.symbol.substr(2)][path] = "";
		    else
			this.parent.gtObject.uiObject[this.symbol.substr(2)][path] = data.d;
		}
	    }
	    else if ( !handlerFlag && this.symbol.indexOf("g.") == 0 )
		this.gtItem.write(
		    null,[{"path" : path.split("."), "data" : data.d}]);
	    this.update(caller);
	},
	update : function(caller) {
	    if ( !this.symbol )
		return;
	    var host = this.parent.getHost();
	    var list = stdObject.copyHash(this.listenerList);
	    for ( ix in list ) {
		var hdr = list[ix];
		if ( hdr.i_removed )
		    continue;
		if ( hdr.listener === caller )
		    continue;
		var ev = new stdEvent("terminalUpdated",host,hdr);
		if ( hdr.handler )
		    hdr.handler.apply(hdr.listener,[ev]);
		else
		    hdr.listener.eventHandler(ev);
	    }
	},
	get : function(path) {
	    if ( !this.symbol )
		return new pigData("error","removed  pigTerminal");
	    if ( this.symbol.indexOf("a.") == 0 ) {
		this.data = new pigData(
		    "pure",
		    this.parent.gtObject.uiObject.getAttribute(
			this.symbol.substr(2)));
	    }
	    else if ( this.symbol.indexOf("g.") == 0 ) {
		if ( this.gtItem.state(["ZOM","FIN"]) )
		    this.data = new pigData("error","data is destroyed");
		else
		    this.data = new pigData("pure",this.gtItem.read());
	    }
	    else if ( this.symbol.indexOf("e.") == 0 ) {
		var ret = new pigData(this.data);
		this.data = new pigData("pure",false);
		ret.setTerminal(this);
		return ret;
	    }
	    else if ( this.symbol.indexOf("p.") == 0 ) {
		this.data = new pigData(
		    "pure",
		    this.parent.gtObject.uiObject[this.symbol.substr(2)]);
		var ret = new pigData(this.data);
		if ( path != "" && path.indexOf(".") < 0 ) {
		    if ( this.symbol == "p.style" )
			ret = new pigData(
			    "pure",
			    pigTerminal.getStyleProperty(
				this.parent.gtObject.uiObject,path));
		    else
			ret = new pigData(
			    "pure",
			    this.parent.gtObject.uiObject[this.symbol.substr(2)][path]);
		}
		ret.setTerminal(this);
		return ret;
	    }
	    var ret = new pigData(this.data.get(path));
	    ret.setTerminal(this);
	    return ret;
	},
	listen : function(listener,handler) {
	    if ( !this.symbol )
		return;
	    if ( !this.parent.getHost() )
		return null;
	    var oid = listener.objId();
	    var hdr = this.listenerList[oid];
	    if ( hdr )
		return hdr;
	    hdr = new pigTerminalHandle(this,listener,handler);
	    this.listenerList[oid] = hdr;
	    return hdr;
	},
	removeListener : function(listener) {
	    if ( !this.symbol )
		return;
	    var oid = listener.objId();
	    delete this.listenerList[oid];
	    if ( this.symbol.indexOf("d.") == 0 &&
		 Object.keys(this.listenerList).length == 0 ) {
		var dv = this.symbol;
		this.destroy();
		delete this.parent.dt[dv];
	    }
	},
	destroy : function() {
	    if ( !this.symbol )
		return;
	    if ( this.symbol.indexOf("e.") == 0 ) {
		this.parent.gtObject.uiObject["on"+this.symbol.substr(2)] 
		    = null;
	    }
	    else if ( this.symbol.indexOf("g.") == 0 ) {
		this.gtItemHandle.remove();
		this.gtItem.releaseRef(this.parent);
	    }
	    var host = this.parent.getHost();
	    var list = stdObject.copyHash(this.listenerList);
	    for ( ix in list ) {
		var hdr = list[ix];
		if ( hdr.i_removed )
		    continue;
		var ev = new stdEvent("terminalDestroyed",host,hdr);
		if ( hdr.handler )
		    hdr.handler.apply(hdr.listener,[ev]);
		else
		    hdr.listener.eventHandler(ev);
	    }
	    this.gtItemHandle = null;
	    this.gtItem = null;
	    this.data = null;
	    this.listenerList = null;
	    this.symbol = null;
	},
    }
);


