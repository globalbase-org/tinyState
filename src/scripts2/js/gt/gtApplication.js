//:IMPORT ts/tinyState.js
//:IMPORT gt/gtItem.js
//:IMPORT std/stdEvent.js
//:EXPORT gtApplication

gtApplication = function(ifclass)
{
    if ( arguments.length == 0 )
        return;
    new stdInterval();
    this.application = this;
    this.ifClass = ifclass;
    this.i_baseURI = {};
    this.i_gtItemURI = {};
    this.i_gtRequestURI = {};
    tinyState.apply(this,[null,1]);
    this.initial("gtApplication");
}

stdObject.defClass
(
    "gtApplication",
    gtApplication,
    tinyState,
    {
	ifClass : null,
	i_baseURI : null,
	i_gtItemURI : null,
	i_gtRequestURI : null,
	i_shared : {},

	getGtPolling : function(uri) {
	    for ( ix in this.i_baseURI ) {
		if ( uri.indexOf(ix,0) != 0 )
		    continue;
		return this.i_baseURI[ix];
	    }
	    return;
	},

	setGtPolling : function(uri,polling) {
	    this.i_baseURI[uri] = polling;
	},

	removeGtPolling : function(uri) {
	    this.i_baseURI[uri] = undef;
	    delete this.i_baseURI[uri];
	},

	getGtRequest : function(uri) {
	    return this.i_gtRequestURI[uri];
	},

	setGtRequest : function(uri,req) {
	    this.i_gtRequestURI[uri] = req;
	},

	removeGtRequest: function(uri) {
	    this.i_gtRequestURI[uri] = null;
	    delete this.i_gtRequestURI[uri];
	},

	getGtItem : function(uri,returnIfNothing) {
	    var ret = this.i_gtItemURI[uri];
	    if ( !ret ||
		 ret.state(["ZOM","FIN"]) ) {
		if ( returnIfNothing == true )
		    return;
		this.i_gtItemURI[uri] = new gtItem(this,uri,ret);
		return this.i_gtItemURI[uri];
	    }
	    return ret;
	},
	getGtItemChildren : function(uri) {
	    var ret = [];
	    for ( uix in this.i_gtItemURI ) {
		if ( uix.indexOf(uri) != 0 )
		    continue;
		ret.push(this.i_gtItemURI[uix]);
	    }
	    return ret;
	},

	sendNoticeToAllGtItem : function() {
	    for ( ix in this.i_gtItemURI ) {
		var g = this.i_gtItemURI[ix];
		g.notice("updated");
	    }
	},

	removeGtItem : function(target,uri) {
	    if ( this.i_gtItemURI[uri] !== target )
		return;
	    this.i_gtItemURI[uri] = null;
	    delete this.i_gtItemURI[uri];
	},

	shared : function(type) {
	    return this.i_shared[type];
	},
	refShared : function(type) {
	    var shared = this.i_shared[type];
	    if ( shared )
		shared.ref ++;
	    else {
		this.i_shared[type] = {
		    "ref" : 1
		};
		shared = this.i_shared[type];
	    }
	    if ( shared.updated )
		shared.updated.wakeup();
	    return shared;
	},
	unrefShared : function(type) {
	    var shared = this.i_shared[type];
	    if ( !shared )
		return;
	    shared.ref --;
	    if ( shared.updated )
		shared.updated.wakeup();
	    if ( shared.ref <= 0 ) {
		delete this.i_shared[type];
	    }
	},
    }
);
