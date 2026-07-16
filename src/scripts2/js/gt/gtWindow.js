//:IMPORT ts/tinyState.js
//:IMPORT gt/gtScope.js
//:EXPORT gtWindow

function gtWindow(invokerObject, parameters) {
    if (arguments.length == 0)
	return;
    var elements = [];
    try {
	elements = parameters.uiObject.document.getElementsByTagName("*");
    } catch (e) {
	this.dump(e + "\n");
    }
    this.i_defaultWindowTitle = parameters.uiObject.document.title;
    this.topLevelElement = elements[0];
    parameters.uiObject.gtPane = this;
    parameters.gid = parameters
	.uiObject.document.documentElement.getAttribute("id");
    if ( !parameters.gid )
        parameters.gid = parameters
	.uiObject.document.documentElement.getAttribute("gid");
    if ( !parameters.gid )
	parameters.gid = "window";
    this.rootScope = true;
    gtScope.apply(this, [invokerObject, elements, true, parameters]);
    this.initial("gtWindow");
}


stdObject.defClass
(
    "gtWindow",
    gtWindow,
    gtScope,
    {
	i_resize : false,

	innerSize : function()
	{
	    var w,h;
	    // IE以外。
	    if ((!document.all || window.opera) && document.getElementById) {
		w=window.innerWidth;
		h=window.innerHeight;
	    }
	    // ウィンドウズIE 6・標準モード。
	    else if (document.getElementById && (document.compatMode=='CSS1Compat')) {
		w=document.documentElement.clientWidth;
		h=document.documentElement.clientHeight;
	    }
	    // その他のIE。
	    else if (document.all) {
		w=document.body.clientWidth;
		h=document.body.clientHeight;
	    }
	    // その他(非対応)。
	    else {
		w=1024;
		h=800;
	    }
	    var ret = {};
	    ret.width = w;
	    ret.height = h;
	    return ret;
	},

	bindWindowParameters : function() {
	    var dt = {};
	    var inner = {};
	    var outer = {};

	    inner = this.innerSize();
	    outer = inner;

	    dt = {};
	    dt.inner = inner;
	    if ( typeof(outer.width) == "number" )
		dt.outer = outer;
	    else
		dt.outer = inner;
	    this.piggybackTurtle.bind(this,"v.windowSize",dt);
	},

	i_critical : false,

	criticalIn : function() {
	    if ( this.i_critical )
		return;
	    this.uiObject.onbeforeunload = function(e){
		return "このページからの移動でサーバとの不整合が発生する可能性があります。"+
		    "現在の処理を終了させてからページ移動することをお勧めします。";
	    };
	    this.i_critical = true;
	},
	criticalOut : function() {
	    if ( !this.i_critical )
		return;
	    this.uiObject.onbeforeunload = null;
	    this.i_critical = false;
	},

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/

	INI_gtScope_END : function(ev) {
	    var gtThis = this;
	    this.uiObject.addEventListener(
		"resize",
		function(event) {
		    gtThis.i_resize = true;
		    gtThis.wakeup();
		},
		true);

	    this.bindWindowParameters();
	    return "doINI_gtWindow_END";
	},
	INI_gtWindow_END : function(ev) {
	    this.i_transaction = this.application.refShared("transaction");
	    this.i_transaction.updated = this;

	    this.i_initInterval = 100000;
	    this.i_initIntervalTime = stdInterval.now() + this.i_initInterval;
	    stdInterval.wait(this,this.i_initInterval,"timer");
	    return "doINI_TINYSTATE_START";
	},

	ACT_TINYSTATE_START : function(ev) {
	    var n = stdInterval.now();
	    if ( this.i_resize || 
		 (this.i_initInterval < 2000000 &&
		  this.i_initIntervalTime - n <= 0) ) {
		this.bindWindowParameters();
		this.i_resize = false;
		this.i_initInterval *= 1.5;
		this.i_initIntervalTime = n + this.i_initInterval;
	    	stdInterval.wait(this,this.i_initInterval,"timer");
	    }
	    if ( this.i_transaction.ref > 1 )
		this.criticalIn();
	    else
		this.criticalOut();
	},
	    
	FIN_START : function(ev) {
	    this.i_transaction.updated = null;
	    this.i_transaction = null;
	    this.application.unrefShared("transaction");
	    return "doFIN_gtScope_START";
	},
    }
);


