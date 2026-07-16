//:IMPORT gt/gtScope.js
//:IMPORT gt/gtRequestSimple.js
//:EXPORT gtFrame

function gtFrame(gtScp,parameters) {
    if ( arguments.length == 0 )
	return;
    this.rootScope = true;
    this.i_destroyDOM_flag = false;
    gtScope.apply(this,[gtScp,null,false,parameters]);
    this.initial("gtFrame");
}


stdObject.defClass
(
    "gtFrame",
    gtFrame,
    gtScope,
    {

	i_src : null,
	i_srcHandle : null,
	i_srcUpdateEvent : null,

	i_srcUpdateHandler : function(ev) {
	    this.i_srcUpdateEvent = ev;
	    this.wakeup();
	},

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/

	INI_gtScope_END : function(ev) {
	    var dt = this.piggybackTurtle.bind(this,"a.src");
	    if ( dt.getType() == "error" )
		return "doFIN_START";
	    this.i_srcHandle = this.piggybackTurtle.refer(this,"a.src",this,this.i_srcUpdateHandler);
	    return "doINI_gtFrame_SRC";
	},
	INI_gtFrame_SRC : function(ev) {
	    var gsrc = this.piggybackTurtle.refer(this,"a.src");
	    if ( gsrc.getType() == "error" )
		gsrc = "";
	    var gsrc = pigActivate.normalize(gsrc);
	    if ( this.i_src == gsrc )
		return "doINI_gtFrame_END";
	    this.i_src = gsrc;
	    if ( this.i_src == "" ) {
		this.i_makeData = "";
		return "doINI_gtFrame_DESTROY_DOM";
	    }
	    if ( this.i_src.indexOf("$") == 0 ) {
		this.i_makeData = "";
		return "doINI_gtFrame_DESTROY_DOM";
	    }
	    if ( this.i_src.indexOf("*") == 0 ) {
		this.i_makeData = this.getFormatDOM(this.i_src.substr(1));
		if ( !this.i_makeData ) {
		    this.dump("NODE "+this.i_src+" is not defiend !! (in "+this.gid+")");
		    this.i_makeData = "";
		    return "doINI_gtFrame_DESTROY_DOM";
		}
		return "doINI_gtFrame_DESTROY_DOM";
	    }
	    return "doINI_gtFrame_ACCESS";
	},
	INI_gtFrame_ACCESS : function(ev) {
	    this.i_req = new gtRequestSimple(
		this,
		"GET",
		this.i_src);
	    return "INI_gtFrame_WAIT";
	},
	INI_gtFrame_WAIT : function(ev) {
	    if ( this.i_destroyFlag ||
		 this.i_destroyed ) {
		this.i_req.destroy();
		return "doFIN_START";
	    }
	    if ( ev.type != "return" )
		return;
	    this.i_makeData = ev.msg.message;
	    return "doINI_gtFrame_DESTROY_DOM";
	},
	INI_gtFrame_DESTROY_DOM : function(ev) {
	    if ( !this.i_destroyDOM_flag )
		return "doINI_gtFrame_MAKE_DOM";
	    this.i_destroyDOM_flag = false;
	    if ( this.i_makeData == "" || (typeof this.i_makeData == "string") )
		this.destroyDOM(false);
	    else
		this.destroyDOM(true);
	    return "doINI_gtFrame_DESTROY_DOM_RET";
	},
	INI_gtFrame_DESTROY_DOM_RET : function(ev) {
	    return this.waitDestroyDOM("doINI_gtFrame_MAKE_DOM");
	},
	INI_gtFrame_MAKE_DOM : function(ev) {
	    if ( this.i_makeData == "" || (typeof this.i_makeData == "string") ) {
		this.makeDOM(this.uiObject,this.i_makeData);
		return "doINI_gtFrame_END";
	    }
	    else {
		this.appendDOMformat(this.uiObject,null,this.i_makeData,
				  {"gid" : "__gtF_"+this.i_src.substr(1)});
		return "doINI_gtFrame_END";
	    }
	},
	INI_gtFrame_END : function(ev) {
	    return "doACT_START";
	},

	ACT_TINYSTATE_START : function(ev) {
	    if ( !this.i_srcUpdateEvent )
		return "ACT_START";
	    this.i_srcUpdateEvent = null;
	    this.i_destroyDOM_flag = true;
	    return "doINI_gtFrame_SRC";
	},

	FIN_START : function(ev) {
	    return "doFIN_gtFrame_START";
	},
	FIN_gtFrame_START : function(ev) {
	    this.i_lastDestroyDOM = false;
	    return "doFIN_gtScope_START";
	},
    }
);
