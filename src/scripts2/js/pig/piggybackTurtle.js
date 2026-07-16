//:IMPORT std/stdObject.js
//:IMPORT pig/pigData.js
//:IMPORT pig/pigTerminal.js

function piggybackTurtle(parent,host) {
    if (arguments.length == 0)
	return;
    if ( parent == "top" )
	parent = null;
    this.parent = parent;
    this.gtObject = null;
    this.gtScope = null;
    this.gtApplication = null;
    this.host = null;
    this.i_keyLength = 0;
    this.functionLockCount = 0;
    this.i_func = {};
    this.refLockCount = 0;
    this.active = true;
    if ( host ) {
	if ( host.isClass("gtObject") ) {
	    this.gtObject = host;
	    if ( host.isClass("gtScope") )
		this.gtScope = host;
	    else
		this.gtScope = host.gtScope;
	}
	else if ( host.isClass("gtApplication") ) {
	    this.gtApplication = host;
	    this.gtScope = this.gtApplication.gtScope;
	}
	else if ( host.isClass("pigPrimitive") ) {
/*
	    var hh = host;
	    for ( ; !hh.isClass("gtObject") ; )
		hh = hh.parent;
	    if ( hh == null )
		this.dump("invalid class object for host(1)");
	    if ( hh.isClass("gtScope") )
		this.gtScope = hh;
	    else
		this.gtScope = hh.gtScope;
*/
	    this.gtScope = parent.gtScope;
	}
	else
	    this.dump("invalid class object for host(2)");
	this.host = host;
    }
    this.dt = {};
    stdObject.apply(this, []);

}


stdObject.defClass
(
    "piggybackTurtle",
    piggybackTurtle,
    stdObject,
    {
	dt : null,

	getHost : function() {
	    if ( this.host )
		return this.host;
	    if ( this.parent )
		return this.parent.getHost();
	    return null;
	},
	getApplication : function() {
	    if ( this.host ) {
		return this.host.application;
	    }
	    if ( this.parent )
		return this.parent.getApplication();
	    return null;
	},

	getGtObject : function() {
	    if ( this.gtObject )
		return this;
	    if ( this.parent )
		return this.parent.getGtObject();
	    return null;
	},

	i_splitPath : function(symbol) {
	    var point = symbol.indexOf("..");
	    if ( symbol.indexOf("..") < 0 )
		return [symbol,"","",""];
	    return [symbol.substr(0,point),
		    symbol.substr(point),
		    symbol.substr(point+1),
		    symbol.substr(point+2)];
	},

	priority : function() {
	    var gtObj = this.getGtObject();
	    if ( !gtObj )
		return tinyState.defaultPriority;
	    return gtObj.gtObject.priority();
	},

	getEnvStack : function() {
	    var env = this;
	    var ret = "";
	    for ( ; env ; env = env.parent ) {
		var item = "(";
		if ( env.host )
		    item += "H:"+env.host.gid+" ";
		if ( env.gtScope )
		    item += "S:"+env.gtScope.gid+" ";
		if ( env.gtObject )
		    item += "O:"+env.gtObject.gid+" ";
		item += ")[";
		var sym
		for ( sym in env.dt ) {
		    item += sym+" ";
		}
		item += "]\n";
		ret += item;
	    }
	    return ret;
	},

	i_regulation : function(caller,symbol) {
	    if ( !symbol ) {
		var msg = "symbol is NULL";
		return new pigData("error",msg);
	    }
	    if ( symbol.indexOf("$",0) == 0 ) {
		var prev = symbol;
		var sp = this.i_splitPath(symbol);
		symbol = this.refer(caller,sp[0].substr(1));
		if ( symbol.getType() == "error" )
		    return symbol;
		if ( symbol.getType() != "string" ) {
		    var msg = "type missmatch ["+prev+"] "+ symbol.getType();
		    return new pigData("error",msg);
		}
		if ( symbol.d.indexOf("..") >= 0 )
		    symbol = symbol.d+sp[2];
		else
		    symbol = symbol.d+sp[1];
	    }
	    if ( symbol.indexOf(gtPolling.URIheader,0) == 0 )
		return "g."+symbol.substr(gtPolling.URIheader.length);
	    if ( symbol.indexOf("a.",0) == 0 ) {
		var pos = symbol.indexOf(":");
		if ( pos < 0 )
		    return symbol.toLowerCase();
		else
		    return symbol.substr(0,pos+1)+symbol.substr(pos+1).toLowerCase();
	    }
	    if ( symbol.indexOf("v.",0) == 0 )
		return symbol;
	    if ( symbol.indexOf("e.",0) == 0 )
		return symbol;
	    if ( symbol.indexOf("g.",0) == 0 )
		return symbol;
	    if ( symbol.indexOf("p.",0) == 0 )
		return symbol;
	    if ( symbol.indexOf("d.",0) == 0 )
		return symbol;
	    return "v."+symbol;
	},
	i_splitName : function(caller,symbol) {
	    symbol = this.i_regulation(caller,symbol);
	    if ( typeof(symbol) != "string" )
		return [null,null,symbol];
	    if ( symbol.indexOf("g.") == 0 ) {
		symbol.split("+").join();
		return ["",symbol,null];
	    }
	    var point = symbol.indexOf(".");
	    var sary = [];
	    sary[0] = symbol.substr(0,point);
	    sary[1] = symbol.substr(point+1);
	    point = sary[1].indexOf(":",0);
	    if ( point < 0 ) {
		symbol.split("+").join();
		return ["",symbol,null];
	    }
	    var nary = [];
	    nary[0] = sary[1].substr(0,point);
	    nary[1] = sary[1].substr(point+1);
	    nary[1] = nary[1].split("+").join();
	    return [nary[0],sary[0]+"."+nary[1],null];
	},

	bind : function(caller,symbol,data) {
	    if ( !this.active )
		return new pigData("error","destroyed environment ("+symbol+")");
	    var sary = this.i_splitName(caller,symbol);
	    if ( sary[2] )
		return sary[2];
	    symbol = sary[1];
	    if ( sary[0] != "" ) {
		if ( !this.gtScope ) {
		    var msg = "undefined gtScope(bind) "+sary[0];
		    return new pigData("error",msg);
		}
		var gt;
		if ( sary[0].indexOf("public") == 0 ) {
		    gt = this.getGtObject();
		    if ( !gt ) {
			var msg = "undefined gt object of "+sary[0];
			return new pigData("error",msg);
		    }
		    gt = gt.gtObject.gtScope.getGtObjectById(sary[0]);
		}
		else if ( sary[0] == ">" ) {
		    var pg = this;
		    var sp = this.i_splitPath(symbol);
		    var sym = sp[0];
		    for ( ; pg && !pg.dt[sym] ; pg = pg.parent );
		    if ( !pg )
			return new pigData("error","no variable "+symbol+" for bind variable");
		    return pg.bind(caller,symbol,data);
		}
		else
		    gt = this.gtScope.getGtObjectById(sary[0]);
		if ( !gt ) {
		    var msg = "undefined gt object(bind) "+sary[0]+" / scope position is "+this.gtScope.gid;
		    return new pigData("error",msg);
		}
		if ( !gt.piggybackTurtle ) {
		    var msg = "undefined environment in gt object(bind) "+sary[0];
		    return new pigData("error",msg);
		}
		return gt.piggybackTurtle.bind(caller,symbol,data);
	    }

	    data = pigData.make(data);

	    if ( symbol.indexOf("e.") == 0 ) {
		if ( data.getType() != "null" ) {
		    var msg = "cannot bind to ev. symbol ["+symbol+"]\n";
		    return new pigData("error",msg);
		}
	    }
	    if ( (symbol.indexOf("a.") == 0 || symbol.indexOf("p.") == 0 ||
		  symbol.indexOf("e.") == 0 ) &&
		 !this.gtObject ) {
		var obj = this.getGtObject();
		if ( !obj ) {
		    var msg  = "not attr(bind) ["+symbol+"] ";
		    return new pigData("error",msg);
		}
		return obj.bind(caller,symbol,data);
	    }
	    var sp = this.i_splitPath(symbol);
	    var sym = sp[0];
	    if ( this.dt[sym] )
		this.dt[sym].set(caller,sp[3],data);
	    else {
                this.dt[sym] = new pigTerminal(this,caller,sp[0],sp[3],data);
		this.i_keyLength ++;
		var dv = "d."+sym.substr(2);
		var term = this.dt[dv];
		if ( sym != dv && term )
		    term.set(caller,"",new pigData("pure",this.i_keyLength));
	    }
	    return new pigData("ok",null);
	},
	refer : function(caller,symbol,listener,handler) {
	    if ( !this.active )
		return new pigData("error","destroyed environment ("+symbol+")");
	    var sary = this.i_splitName(caller,symbol);
	    if ( sary[2] )
		return sary[2];
	    symbol = sary[1];
	    if ( sary[0] != "" ) {

		if ( !this.gtScope ) {
		    var msg = "(caller:"+caller.className+") undefined gtScope "+sary[0];
		    return new pigData("error",msg);
		}
		var gt;
		if ( sary[0].indexOf("public") == 0 ) {
		    gt = this.getGtObject();
		    if ( !gt ) {
			var msg = "(caller:"+caller.className+")undefined gt object of "+sary[0];
			return new pigData("error",msg);
		    }
		    gt = gt.gtObject.gtScope.getGtObjectById(sary[0]);
		}
		else
		    gt = this.gtScope.getGtObjectById(sary[0]);
		if ( !gt ) {
		    var msg = "(caller:"+caller.className+") undefined gt object (refer) "+sary[0]+" scope = "+this.gtScope.gid+" "
			+this.gtScope.getGtObjectIdList()+" "+this.gtScope.state();
		    return new pigData("error",msg);
		}
		if ( !gt.piggybackTurtle ) {
		    var msg = "(caller:"+caller.className+") undefined environment in gt object "+sary[0];
		    return new pigData("error",msg);
		}
		var ret = gt.piggybackTurtle.refer(caller,symbol,listener,handler);
		if ( ret.getType() != "error" )
		    return ret;
		if ( this.host ) {
		    ret.d += "(r1:GID="+this.host.gid+")";
		}
		else {
		    ret.d += "(r1)";
		}
		return ret;
	    }
	    var sp = this.i_splitPath(symbol);
	    if ( this.dt[sp[0]] ) {
		if ( listener == "point" ) 
		    return pigData("pn:piggybackTurtle",this);
		if ( listener ) {
		    var hdr = this.dt[sp[0]].listen(listener,handler);
		    return  new pigData("pn:terminalHandle",hdr);
		}
		if ( (!this.host && caller.sync ) || 
		     (this.host && caller.sync && caller.sync != this.host.sync) )
		    caller.sync.set(this.dt[sp[0]]);
		var ret = this.dt[sp[0]].get(sp[3]);
		return ret;
	    }
	    if ( !this.parent ) {
		var msg = "(caller:"+caller.className+") ["+symbol+"] is not defined\n";
		return new pigData("error",msg);
	    }
	    var ret = this.parent.refer(caller,symbol,listener,handler);
	    if ( ret.getType() != "error" )
		return ret;
	    if ( this.host ) {
		ret.d += "(r2:GID="+this.host.gid+")";
	    }
	    else {
		ret.d += "(r2)";
	    }
	    return ret;
	},
	del : function(caller,symbol) {
	    if ( !this.active )
		return new pigData("error","destroyed environment ("+symbol+")");
	    var sary = this.i_splitName(caller,symbol);
	    if ( sary[2] )
		return sary[2];
	    symbol = sary[1];
	    if ( sary[0] != "" ) {
		if ( !this.gtScope ) {
		    var msg = "undefined gtScope "+sary[0];
		    return new pigData("error",msg);
		}
		var gt;
		if ( sary[0].indexOf("public") == 0 ) {
		    gt = this.getGtObject();
		    if ( !gt ) {
			var msg = "undefined gt object of "+sary[0];
			return new pigData("error",msg);
		    }
		    gt = gt.gtObject.gtScope.getGtObjectById(sary[0]);
		}
		else
		    gt = this.gtScope.getGtObjectById(sary[0]);
		if ( !gt ) {
		    var msg = "undefined gt object "+sary[0];
		    return new pigData("error",msg);
		}
		if ( !gt.piggybackTurtle ) {
		    var msg = "undefined environment in gt object "+sary[0];
		    return new pigData("error",msg);
		}
		return gt.piggybackTurtle.del(caller,symbol);
	    }
	    if ( this.dt[symbol] ) {
		var tr = this.dt[symbol];
		if ( this.dt ) {
		    this.dt[symbol] = null;
		    delete this.dt[symbol];
		}
		tr.destroy();
		return new pigData("null");
	    }
	    if ( !this.parent ) {
		var msg = "["+symbol+"] is not defined(del)\n";
		return new pigData("error",msg);
	    }
	    return this.parent.del(caller,symbol);
	},
	destroy : function() {
	    if ( this.refLockCount )
		return;
	    this.active = false;
	    var dtlist = this.dt;
	    this.dt = [];
	    for ( ix in dtlist ) {
		dtlist[ix].destroy();
	    }
	    this.dt = null;
	    this.gtObject = null;
	    this.gtApplication = null;
	    this.host = null;
	    this.parent = null;
	    this.i_func = null;
	},

	refLock : function() {
	    var pig = this;
	    for ( ; pig ; ) {
		if ( pig.gtObject )
		    return;
		pig.refLockCount ++;
		pig = pig.parent;
	    }
	},

	refUnlock : function() {
	    var pig = this;
	    for ( ; pig ; ) {
		if ( pig.gtObject )
		    return;
		pig.refLockCount --;
		if ( pig.refLockCount == 0 )
		    pig.destroy();
		pig = pig.parent;
	    }
	},

	setFunction : function(func) {
	    this.i_func[func.seq()] = func;
	},
	functionLock : function() {
	    this.functionLockCount ++;
	},
	functionUnlock : function() {
	    this.functionLockCount --;
	    if ( this.functionLockCount == 0 && this.host )
		this.host.wakeup();
	},
	functionAbort : function() {
	    var ix;
	    for( ix in this.i_func ) {
		this.i_func[ix].functionAbort();
	    }
	},
	functionRefUnlock : function() {
	    var ix;
	    for( ix in this.i_func ) {
		this.i_func[ix].d.env.refUnlock();
		delete this.i_func[ix];
	    }
	}
    }
);


piggybackTurtle.top = new piggybackTurtle("top",null);

