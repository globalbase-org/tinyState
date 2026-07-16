//:IMPORT ts/tinyState.js

function pigData(type,arg1,arg2,arg3) {
    if (arguments.length == 0)
	return;
    if ( typeof(type) == "object" ) {
	this.type = type.type;
	this.d = type.d;
	this.sub = type.sub;
	this.trace = type.trace;
    }
    else if ( type == "pn:lambda" ) {
	this.type = type;
	this.d = {};
	this.d.org = this.d;

	this.d.env = arg1;
	this.d.args = arg2;
	this.d.body = arg3;
	this.d.active = true;
	if ( arg1 ) {
	    this.d.hostEnv = arg1.getGtObject();
	    this.d.execution = {};
	    this.d.hostEnv.setFunction(this);
	    this.d.env.refLock();
	}
    }
    else if ( type == "pure" ) {
	this.type = type;
	this.d = arg1;
	this.sub = arg2;
    }
    else {
	this.type = type;
	this.d = arg1;
	this.sub = arg2;
    }
    this.trace = false;
    stdObject.apply(this);
}


pigData.make = function(dd) {
    if ( dd === null )
	return new pigData("pure",null);
    if ( typeof(dd) != "object" )
	return new pigData("pure",dd);
    if ( dd.isClass && dd.isClass("pigData") )
	return dd;
    return new pigData("pure",dd);
}


pigData.i_set = function(ptr,path,data) {
    var point = path.indexOf(".");
    if ( point < 0 ) {
	ptr[path] = data;
	return;
    }
    var element = path.substr(0,point);
    var next = path.substr(point+1);
    if ( typeof(ptr[element]) != "object" )
	ptr[element] = {};
    pigData.i_set(ptr[element],next,data);
}

pigData.i_get = function(ptr,path) {

    if ( path == "" )
	return new pigData("pure",ptr);
    if ( ptr == null )
	return new pigData("pure",null);
    var point = path.indexOf(".");
    var element;
    var next;
    if ( point < 0 ) {
	element = path;
	next = "";
    }
    else {
	element = path.substr(0,point);
	next = path.substr(point+1);
	if ( ptr == null || typeof(ptr) != "object" )
	    return new pigData("pure",null);
	if ( typeof(ptr[element]) != "object" )
	    return new pigData("pure",null);
    }
    return pigData.i_get(ptr[element],next);
}

stdObject.defClass
(
    "pigData",
    pigData,
    stdObject,
    {
	lock : function(funcObj) {
	    if ( this.type != "pn:lambda" )
		return;
	    if ( !this.d.env )
		return;
	    this.d.org.hostEnv.functionLock();
	    this.d.org.execution[funcObj.objId()] = funcObj;
	},
	unlock : function(funcObj) {
	    if ( this.type != "pn:lambda" )
		return;
	    if ( !this.d.env )
		return;
	    delete this.d.org.execution[funcObj.objId()];
	    this.d.org.hostEnv.functionUnlock();
	},
	functionAbort : function() {
	    if ( this.type != "pn:lambda" )
		return;
	    this.d.active = false;
	    var ix;
	    for ( ix in this.d.execution )
		this.d.execution[ix].abort();
	},

	getType : function() {
	    if ( this.type == "pure" ) {
		if ( this.d == null )
		    return "null";
		if ( Array.isArray(this.d) )
		    return "array";
		return typeof(this.d);
	    }
	    return this.type;
	},

	setTerminal : function(term) {
	    this.terminal = term;
	},

	set : function(path,data) {
	    if ( path == "" ) {
		this.d = data.d;
		this.type = data.type;
		return this;
	    }
	    if ( this.type != "pure" )
		return this;
	    pigData.i_set(this.d,path,data.d);
	    return this;
	},

	get : function(path) {
	    if ( path == "" )
		return this;
	    if ( this.type != "pure" )
		return new pigData("pure",null);
	    return pigData.i_get(this.d,path);
	}
    }
);


